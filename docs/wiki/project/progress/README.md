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

A file's score is (done + chosen + the partial rows' shares) / (all its rows) (user, 2026-10-21: a partial row counts
its own share, no longer a half, and what is only inferred counts nothing until it is checked). Write a partial row as
`partial 40%` with what is missing in its notes, and an inferred one with what is unverified; the dashboard shows the
inferred rows and the partial rows not measured yet apart, so the work of checking them can be seen.

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

## Tools

- `python docs/tools/progress_score.py <file>...` rewrites each file's Progress line from its rows.
- `python docs/tools/progress_index_regen.py docs/wiki/project/progress` regenerates `INDEX.md`, sub-folders included.
- `python docs/tools/vanilla_bugs_count.py docs/wiki/project/progress/vanilla_bugs` rewrites the bug files' count lines.
