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
  (`docs/wiki/project/dashboard.md`), the open rows of the port audit and, when given, the issues and raffclar data.
- `progress_score.py <file>...`: rewrites a progress file's Progress line from its rows.
- `progress_index_regen.py docs/wiki/project/progress`: regenerates the progress `INDEX.md`, sub-folders included.
- `vanilla_bugs_count.py docs/wiki/project/progress/vanilla_bugs`: rewrites the bug files' count lines.

To read the wiki locally, run `docs/open_wiki.bat` (or `python docs/serve.py`): it serves `docs/` on 127.0.0.1 and
opens the viewer.
