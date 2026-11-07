# Wiki tools

Scripts for the wiki in `docs/wiki` and its viewer `docs/index.html`. Standard library only; run them with
`python -I` from the repository root.

- `links.py docs`: checks every relative link and `#anchor` of `docs/SUMMARY.md` and the pages under `docs/wiki`
  with GitHub's heading slugs; exits 1 if one is broken.
- `coverage.py [--drop docs/wiki/project/DROPPED.tsv] <old dir or file>... docs/wiki`: proves that every fact (address,
  function name, number with a unit) of older documentation is in the wiki, or dropped on purpose with a reason.
- `status_marks.py dry|run docs/wiki`: gives the evidence marks their emoji (faithful, approximate, inferred, verify,
  1.20 only, chosen difference); the word always stays.
- `progress_stats.py [--issues <file.jsonl>] [--raffclar <clone>]`: recounts the progress dashboard
  (`docs/wiki/project/dashboard.md`) and its SVG bars (`docs/wiki/project/bars/`), the generated part of
  `raffclar-status.md` (from `raffclar-status.tsv`), the latest milestones (from `milestones.md`), the open rows of the
  port audit, the refactor table's test row (counted from `test/`), the progress block of the repository's
  `readme.md` and, when given, the issues and his master (`raffclar.json`, and his new pull requests into
  `raffclar-status.tsv` as pending).
- `progress_rows.py`: the status rule the three progress tools share: done, chosen, partial NN%, inferred, pending,
  and the score (done + chosen + the partial shares) / all rows.
- `progress_groups.py`: how the dashboard groups the rows (the families' save and load, the story by land, the
  parked online rows, and the done rows with and without a test); the rules are in
  `docs/wiki/project/progress/README.md#how-the-dashboard-groups-the-rows`.
- `progress_score.py <file>...`: rewrites a progress file's Progress line from its rows.
- `progress_index_regen.py docs/wiki/project/progress`: regenerates the progress `INDEX.md`, sub-folders included.
- `vanilla_bugs_count.py docs/wiki/project/progress/vanilla_bugs`: rewrites the bug files' count lines.
- `cpp_counts.py <repository>`: counts the C arrays, the `new` / `delete` expressions and the direct file reads
  (`std::ifstream`, `fopen`) under `src/`, comments and literals left out, for the refactor table's rows; lists each and prints the counts last.
- `mutable_data.py <dumpbin.exe> <objects folder>...`: counts the game code's mutable globals and statics from a
  Release build's object files (MSVC's dumpbin), for the refactor table's row; lists each and prints the count last.

To read the wiki locally, run `docs/open_wiki.bat` (or `python docs/serve.py`): it serves `docs/` on 127.0.0.1 and
opens the viewer.
