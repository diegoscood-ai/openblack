# openblack progress

> These notes were written by raffclar ([raffclar/openblack](https://github.com/raffclar/openblack)) and given to
> this fork. Their statuses are re-marked for this tree: what this tree's code does, not his branch.

How much of Black & White (2001) openblack does, domain by domain. Each folder is one domain of the game; each file in
it is one feature, broken down into everything the original game does for that feature, so it can be ticked off.

Measured against this fork's `local/hand-hbn` at `6dcd1fdc` (2026-10-20, joint 147); raffclar first measured them against his
`miracles-rework` at `40d5d371`. Update a row's status in the same
change that does the work.

## Status

| Status            | Meaning                                                                                  | Counts |
|-------------------|------------------------------------------------------------------------------------------|--------|
| **done**          | Works as the original game does, checked against it                                       | 1 |
| **chosen**        | Done the way the user chose: a dated chosen difference ([reconstruction.md](../reconstruction.md#what-is-deliberately-different--chosen-differences)) | 1 |
| **partial NN%**   | Part of it works, checked against the original: NN of 100 of the behaviour; the notes say what is missing | NN/100 |
| **partial**       | Part of it works, not measured yet (to be given its NN% when it is next read against the original) | 0 |
| **inferred**      | Written from a guess or an inference, not checked against the original; the notes say what is unverified | 0 |
| **pending**       | Not started (older branches say `todo`, which reads the same)                             | 0 |
| **device**        | The original's behaviour that only a force-feedback device reaches: the original's script gate `IMMERSION_EXISTS` answers no without one, as openblack's does (user, 2026-10-27) | not scored |
| **per use**       | The summary row of a native counted per use (below): its uses are rows of their own, each with its status | not scored |

A file's score is (done + chosen + the partial rows' shares) / (all its rows but the device-only ones) (user, 2026-10-21: a partial row counts
its own share, no longer a half, and what is only inferred counts nothing until it is checked). Write a partial row as
`partial 40%` with what is missing in its notes, and an inferred one with what is unverified; the dashboard shows the
inferred rows and the partial rows not measured yet apart, so the work of checking them can be seen.

A native that does something different for each use (a type, a mode, a property) is counted per use: each use is a row
of its own, with its own status, and the native's row stays where it was as their summary, with the status
**per use**, which counts in no score (2026-10-29; the first are CREATE, OBJECT_DELETE and the script properties on
[the objects natives](scripts/challenge_natives_objects_and_world.md#per-use)). A quest row cites the use it rests on.

A story or quest row is closed by a run's log, never by eye: `tools/checks/story/quest_check.py` reads the log of a
run with the native trace (a natural run, or a quest run through a land entry's `quest=`) against the quest's spec, and
the row moves only in a commit that cites its PASS. A PASS shows the quest's script reached that beat with those
arguments; the effect the row describes (the camera's flight, a creature's moves, a sound) belongs to its own system.
So a quest row moves to **done** on a PASS only when that effect is done in its own system's row, cited in the commit;
otherwise it moves to **partial NN%**, and its note names the system row that would finish it. A row about how the
story reaches the quest (its start gate) closes only from a natural run (2026-10-27).

Behaviour the original does not have (written but never run, cut, served by online services that are gone, a
developer tool, or an earlier guess the original did not bear out) gets **no row** (user, 2026-10-21): it is not
progress. What was found about it goes on [Not in the original](../not-in-the-original.md), which is read before a
row is added. The older status n/a is still read by the tools, but no row is written with it.

## File format

```markdown
# <Feature>

<One or two sentences: what the feature is in the game.>

**Progress: <done>/<total> done, <chosen> chosen, <partial> partial, <inferred> inferred — <score>%**

## <Part of the feature>

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| <one thing the game does, in plain words> | done | `src/...` (system or file); test name if any |
```

- One row per behaviour a player could notice, or a rule the game follows, not one row per class.
- Notes say where openblack does it (paths under `src/`), or what is missing.
- Plain English: no addresses, no decompiled or internal names of the original.
- A file about work done in another codebase says so under its title, e.g. `**Codebase: bwgame-service**
  (C:\projects\bwgame-service)` for the online servers and web site, and its notes start with that codebase's name
  (`bwgame-service: online/clans.py`). Its rows count in the scores like any other.

## Domains

See [INDEX.md](INDEX.md) for every file and its score.

Bugs in the original game that the research has found are logged, unscored, in [vanilla_bugs/](vanilla_bugs/README.md).

## How the dashboard groups the rows

The [dashboard](../dashboard.md) counts every row once, under one line; no rule here changes a row's status
(user, 2026-10-24). `docs/tools/progress_groups.py` holds the rules, in this order:

1. **Online, parked**: [network play](multiplayer/network_play.md), the
   [multiplayer rules](multiplayer/multiplayer_rules.md) and the [online services](multiplayer/online_services.md),
   and the online sections of [Online services and PC extras](pc_integration/online_services.md) (Connecting,
   Uploading the creature, Creatures in online games, Maps from the internet) are counted on a line of their own and
   in no score (online waits until the game works offline). So is [Firestorm](multiplayer/maps/firestorm.md), a copy
   of the game's online map `Online Maps/mpm_3p_2.map`: its page and every section of the
   [playground scripts](scripts/playground_scripts.md) whose heading starts "Firestorm:" (2026-10-26). Skirmish, the
   other multiplayer lands, diplomacy, the creature's web page and the force-feedback mouse are offline and stay in
   their domains.
2. **Save and load**: every row of [saving and loading](engine/saving_and_loading.md), and every row of another
   domain whose behaviour cell is about the saved game: "saved and loaded", "saved with the game", "kept in a saved
   game", "a saved game keeps", "saved games", saving the game, the quick saves and the automatic saves. Each goes to
   one of three parts by its behaviour cell: *loading a save* when it names only reading one back (loads, restores,
   puts back), *writing a save* when it names only writing one (saves, is saved into, writes), and *kept in the save*
   when it names both, or keeps a thing in the saved game or with the game (one row for both halves), or names
   neither (what a save holds). A row for both halves stays one row: to count its halves apart, split it into a
   loading row and a writing row.
3. **The story, land by land**: each story row goes to the land whose section of
   [gold_scrolls.md](story/gold_scrolls.md) or [silver_scrolls.md](story/silver_scrolls.md) lists its file (a file
   listed under two lands goes by its sections), or to Land N for `land_N.md`; otherwise to the one land its section
   heading names ("Land 4's totems", "(land 1)", "The tutorial island"), otherwise to the page's own land (the
   tutorial's opening is Land 1's), otherwise across the lands. A row is a gold scroll's or a silver scroll's when its
   file is in that index or its folder, and the rest otherwise.
4. Every other row counts in its own domain, and its domain in its family.

**Done with a test.** A done row is *with a test* when its notes name one of the tests under `test/`: a test's name in
backticks whose suite is one of `test/`'s (`` `VillagerFoodTest.GetFoodFromHomeTakesTwice` ``, `` `SavedGameLoad.*` ``),
the word test or tests before a name in backticks (test `` `X` ``), a test source (`test/…`, `test_….cpp`), or a
reference to a test named above it in the file ("same test", "these tests", "tests as above"). A testbed scenario, a
scripted hand demo or a check run (fidelity, cycle) is not a test for this count. Write a row's test by its gtest name so that it counts.

**Done in play.** A test can inject the action it checks, so a test alone does not show the behaviour works in real
play. A done row is also *proven in play* when its notes cite a run played with real input: a hand-played run's log
under `refs/story/played/` or a promoted real-input run's under `refs/realinput/`, as
`` refs/story/played/<run>.log line N `` or `` refs/realinput/<scenario>.log line N ``. A row can be both with a test and
in play; it counts in each of the two columns, the summary says how many have both, and *neither* counts the done rows
with no test and no play evidence, so with a test + in play − both + neither = done. A scripted hand demo or a check
run is not play evidence. The score does not change with any of these.

**Playable by land** is not counted: a land is playable when every system it uses works, and the rows do not say
which lands use which system, so the land lines count the story's rows only.

## Tools

- `python docs/tools/progress_score.py <file>...` rewrites each file's Progress line from its rows.
- `python docs/tools/progress_index_regen.py docs/wiki/project/progress` regenerates `INDEX.md`, sub-folders included.
- `python docs/tools/vanilla_bugs_count.py docs/wiki/project/progress/vanilla_bugs` rewrites the bug files' count lines.
