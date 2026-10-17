# openblack wiki

How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.

- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse [the pages](SUMMARY.md) here.
- **Start:** the [progress dashboard](wiki/project/dashboard.md) (live bars in the viewer) and [Reading this wiki](wiki/project/reading-the-wiki.md).

*This page is written by `tools/progress_stats.py`.*


## Progress

*Written by `tools/progress_stats.py`; the viewer draws the same counts live.*

### Features

**[Overall](wiki/project/progress/INDEX.md): 46%**: 5290 done, 2183 partial, 6477 todo (1046 n/a left out) across 14996 rows in 377 files.

`███████████████▒▒▒▒▒▒░░░░░░░░░░░░░░░░░░░`

█ done · ▒ partial · ░ todo

| Domain | Progress | Score | Done / partial / todo | Issues |
|---|---|---:|---|---|
| [ocean](wiki/project/progress/INDEX.md#ocean--94) | `███████████████████░` | 94% | 53 / 1 / 3 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [audio](wiki/project/progress/INDEX.md#audio--88) | `████████████████▒▒▒░` | 88% | 84 / 13 / 6 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [sky](wiki/project/progress/INDEX.md#sky--87) | `█████████████████▒░░` | 87% | 154 / 12 / 18 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [physics](wiki/project/progress/INDEX.md#physics--84) | `████████████████▒▒░░` | 84% | 263 / 43 / 33 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics,area:particles) |
| [gesture](wiki/project/progress/INDEX.md#gesture--83) | `████████████████▒░░░` | 83% | 95 / 9 / 16 | [10 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:magic) |
| [animal](wiki/project/progress/INDEX.md#animal--82) | `████████████████▒░░░` | 82% | 70 / 7 / 13 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animals) |
| [terrain](wiki/project/progress/INDEX.md#terrain--82) | `███████████████▒▒▒░░` | 82% | 88 / 21 / 11 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [debug](wiki/project/progress/INDEX.md#debug--78) | `██████████████▒▒▒░░░` | 78% | 155 / 33 / 32 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:editor) |
| [miracles](wiki/project/progress/INDEX.md#miracles--78) | `██████████████▒▒▒░░░` | 78% | 560 / 100 / 127 | [10 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:magic) |
| [scripts](wiki/project/progress/INDEX.md#scripts--74) | `██████████████▒▒░░░░` | 74% | 1084 / 191 / 328 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:script) |
| [resources](wiki/project/progress/INDEX.md#resources--72) | `█████████████▒▒░░░░░` | 72% | 83 / 13 / 28 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [video](wiki/project/progress/INDEX.md#video--72) | `█████████████▒▒▒░░░░` | 72% | 53 / 13 / 17 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [nature](wiki/project/progress/INDEX.md#nature--71) | `█████████████▒▒░░░░░` | 71% | 161 / 21 / 58 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:trees) |
| [weather](wiki/project/progress/INDEX.md#weather--71) | `█████████████▒▒░░░░░` | 71% | 83 / 12 / 31 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:weather) |
| [rendering](wiki/project/progress/INDEX.md#rendering--69) | `████████████▒▒▒░░░░░` | 69% | 108 / 32 / 41 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [temple](wiki/project/progress/INDEX.md#temple--68) | `████████████▒▒▒░░░░░` | 68% | 57 / 17 / 22 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:buildings) |
| [building](wiki/project/progress/INDEX.md#building--65) | `███████████▒▒▒▒░░░░░` | 65% | 134 / 44 / 61 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:buildings) |
| [camera](wiki/project/progress/INDEX.md#camera--65) | `██████████▒▒▒▒▒▒░░░░` | 65% | 99 / 49 / 41 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [hand](wiki/project/progress/INDEX.md#hand--65) | `████████████▒▒░░░░░░` | 65% | 387 / 94 / 190 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [worship](wiki/project/progress/INDEX.md#worship--65) | `███████████▒▒▒▒░░░░░` | 65% | 77 / 21 / 37 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villagers) |
| [engine](wiki/project/progress/INDEX.md#engine--61) | `█████████▒▒▒▒▒▒░░░░░` | 61% | 116 / 79 / 60 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [town](wiki/project/progress/INDEX.md#town--59) | `██████████▒▒▒░░░░░░░` | 59% | 109 / 31 / 70 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [villager](wiki/project/progress/INDEX.md#villager--59) | `██████████▒▒▒░░░░░░░` | 59% | 198 / 53 / 130 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villagers) |
| [creature](wiki/project/progress/INDEX.md#creature--48) | `████████▒▒▒▒░░░░░░░░` | 48% | 630 / 301 / 683 | [13 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [multiplayer](wiki/project/progress/INDEX.md#multiplayer--46) | `███████▒▒▒▒░░░░░░░░░` | 46% | 116 / 68 / 143 |  |
| [interface](wiki/project/progress/INDEX.md#interface--42) | `█████▒▒▒▒▒▒░░░░░░░░░` | 42% | 89 / 95 / 144 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [pc_integration](wiki/project/progress/INDEX.md#pc_integration--20) | `███▒▒░░░░░░░░░░░░░░░` | 20% | 27 / 9 / 120 |  |
| [rival_gods](wiki/project/progress/INDEX.md#rival_gods--17) | `██▒▒▒░░░░░░░░░░░░░░░` | 17% | 41 / 56 / 320 |  |
| [easter_eggs](wiki/project/progress/INDEX.md#easter_eggs--13) | `█▒▒▒░░░░░░░░░░░░░░░░` | 13% | 5 / 9 / 59 |  |
| [story](wiki/project/progress/INDEX.md#story--11) | `▒▒▒▒░░░░░░░░░░░░░░░░` | 11% | 111 / 736 / 3635 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:script) |

### Unification with raffclar's tree

Full unification means the port target closed and his newer commits audited.

**raffclar's newer work brought in: 101 of 704.5 rows of gain: 14%**

`██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░`

9 of his 189 pull requests with a gain are ported, lane by lane (commits marked "Ported from raffclar/openblack PR #N", unverified against v1.42 until the audit). The gain of each is in [raffclar_uplift.tsv](wiki/project/raffclar_uplift.tsv); importing all of it would take the features score from 44.8% to about 49.7%.

**raffclar's commits inside our port target: 1084 of 3351: 32%**

`█████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░`

Counted from upstream openblack's `a243c6b2` to his master `aba44a4f` of 2026-10-17 (computed 2026-10-17); the target is `0222f9f5`.

**Port rows of the target closed: [105 of 161 rows closed](wiki/project/port-audit.md), 56 open: 65%**

`██████████████████████████░░░░░░░░░░░░░░`

| Area | Progress | Closed | Open | Issues |
|---|---|---:|---:|---|
| [Creature, magic and camera](wiki/project/port-audit.md#creature-magic-and-camera) | `█████████████░░░░░░░` | 30 of 47 | 17 | [26 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature,area:magic,area:camera) |
| [Renderer and interface](wiki/project/port-audit.md#renderer-and-interface) | `█████████████████░░░` | 36 of 42 | 6 | [17 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering,area:interface,area:editor,area:video,area:weather) |
| [Villagers, scripts, hand and audio](wiki/project/port-audit.md#villagers-scripts-hand-and-audio) | `████████████░░░░░░░░` | 28 of 45 | 17 | [34 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villagers,area:script,area:hand,area:audio,area:particles,area:town,area:buildings) |
| [Testbed, physics and animals](wiki/project/port-audit.md#testbed-physics-and-animals) | `████████░░░░░░░░░░░░` | 9 of 24 | 15 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:testbed,area:physics,area:animals,area:trees) |
| [Coordination](wiki/project/port-audit.md#coordination) | `█████████████░░░░░░░` | 2 of 3 | 1 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |

[2267 newer commits on his master after the target (1172 merges), not audited yet](https://github.com/raffclar/openblack/compare/0222f9f5...master).

### Refactor

| Metric | Progress | Done | Before → now | What is left |
|---|---|---:|---|---|
| [Mutable globals and file- or function-scope statics in game code](wiki/project/refactor-conventions.md#progress-counts) | `██████████████████░░` | ~89% | 185 → 20 | **Partly an accepted exception.** Mostly log-once warning flags inside functions, the `Game` instance pointer, and three file-scope objects; a handful stay on purpose (see below) |
| [`Game::Instance()` call sites](wiki/project/refactor-conventions.md#progress-counts) | `████████████████████` | ~98% | 89 → 2 | Two in the debug UI: loading a map and reading the turn delta. Both need `Game` split further |
| [Locator services](wiki/project/refactor-conventions.md#progress-counts) | (a count, no goal) | — | 29 → 81 | New services replaced the globals; the number grows as state moves |
| [Plain `enum` declarations](wiki/project/refactor-conventions.md#progress-counts) | `████████████████████` | 100% | 14 files → 0 | Bit-flag enums became `constexpr` constants with the same type and values |
| [`new` / `delete` in game code](wiki/project/refactor-conventions.md#progress-counts) | `█████████████████░░░` | ~84% | 43 → 7 | **Accepted exception.** Required by a C API or a private constructor |
| [C arrays](wiki/project/refactor-conventions.md#progress-counts) | `████████████████░░░░` | ~82% | 44 → 8 | **Accepted exception.** Required by a C API (`argv`, OS buffers, a union member) |
| [Direct file reads (`std::ifstream`, `fopen`)](wiki/project/refactor-conventions.md#progress-counts) | `███████████░░░░░░░░░` | ~55% | 20 → 9 | **Accepted exception.** All of them out of scope: the file system itself, the crash handler, a user save, streaming video and a bank fallback |
| [Comments naming the original binary (`fn_…`)](wiki/project/refactor-conventions.md#progress-counts) | `████████████████████` | 100% | 5354 → 0 | Across `src`, `components` and the shaders. The detail lives in this wiki |
| [Comments quoting the original's addresses](wiki/project/refactor-conventions.md#progress-counts) | `████████████████████` | 100% | — → 0 | The 19 hexadecimal values left in comments are data: colours, masks, flags, file-format offsets |
| [Identifiers named after the original binary or unclear](wiki/project/refactor-conventions.md#progress-counts) | `█████████████████░░░` | ~84% | 330 to rename → 41 of 49 batches done | **Partly an accepted exception.** Seven batches left, listed in order; 161 names that come from upstream openblack are left alone on purpose |
| [Files that fail the CI's clang-format 17 check](wiki/project/refactor-conventions.md#progress-counts) | `████████████████████` | 100% | ~700 → 0 | One formatting commit, then every later change formatted on the files it touched |
| [Test files](wiki/project/refactor-conventions.md#progress-counts) | (a count, no goal) | — | 151 → 165 | Grouped into 13 executables; 46 tests gained a synthetic twin. 1487 tests pass; 53 skip when the game data they need is not found |
