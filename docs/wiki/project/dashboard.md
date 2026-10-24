# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard
features: progress/INDEX.md
port: port-audit.md
refactor: refactor-conventions.md#progress-counts
issues: issues.json
raffclar: raffclar.json
-->

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟨🟪⬜⬜⬜⬜ 55%](bars/features-all.svg?v=74a05c4d)

6279 of the original game's 13975 behaviours are done and 5 done as the user chose; 2504 partial (10% of the rows' worth, 237 not measured yet); 240 inferred, not yet checked against the original.

**🔀 raffclar's work in our tree**

![🟩🟩⬜⬜⬜⬜⬜⬜⬜⬜ 21%](bars/raffclar-all.svg?v=5dfc5007)

132 of his 783 pull requests are in our tree, 60 more in part.

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing.

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 89%](bars/family-world.svg?v=a0b7f50e) | 944 | 2 | 166 (123.2 done) | 24 | 69 |
| 🐾 Beings | ![🟩🟩🟩🟩🟩🟩🟨🟪⬜⬜ 71%](bars/family-beings.svg?v=e184da68) | 1509 | 0 | 598 (372.4 done) | 158 | 380 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](bars/family-player-and-interface.svg?v=ad94b095) | 1634 | 2 | 524 (295.6 done) | 54 | 345 |
| 📜 Story | ![🟩🟩🟨⬜⬜⬜⬜⬜⬜⬜ 31%](bars/family-story.svg?v=954a115b) | 1567 | 0 | 1050 (467.3 done) | 1 | 3968 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 74%](bars/family-engine.svg?v=bea2aed5) | 625 | 1 | 166 (95.3 done) | 3 | 185 |

<details><summary>Every domain</summary>

| Domain | Progress | Done | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 59 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-sky.svg?v=67e1023d) | 181 | 2 | 2 (1.9 done) | 0 | 2 |  |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-terrain.svg?v=76ef8f39) | 109 | 0 | 7 (5.2 done) | 0 | 2 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/features-weather.svg?v=023fe5ee) | 115 | 0 | 3 (0.8 done) | 0 | 5 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:weather) |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/features-physics.svg?v=36f42d0d) | 269 | 0 | 69 (50.9 done) | 5 | 9 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics) |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](bars/features-resources.svg?v=23d76bb8) | 76 | 0 | 29 (21.9 done) | 3 | 16 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:resources) |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟨🟪⬜⬜ 73%](bars/features-nature.svg?v=0015d9a8) | 135 | 0 | 56 (42.6 done) | 16 | 35 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:nature) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟨🟨🟪⬜ 84%](bars/features-animal.svg?v=0b76b69f) | 54 | 0 | 27 (21.5 done) | 3 | 6 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](bars/features-worship.svg?v=f3421b18) | 101 | 0 | 24 (16.7 done) | 2 | 16 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 80%](bars/features-building.svg?v=1a800853) | 154 | 0 | 69 (47.0 done) | 3 | 26 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 79%](bars/features-villager.svg?v=092e5d59) | 270 | 0 | 33 (18.0 done) | 5 | 57 | [12 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](bars/features-town.svg?v=2d91029b) | 120 | 0 | 49 (33.9 done) | 2 | 34 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 66%](bars/features-creature.svg?v=c0da3c8b) | 810 | 0 | 396 (235.3 done) | 143 | 241 | [24 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/features-gesture.svg?v=69b557dc) | 107 | 0 | 2 (1.8 done) | 0 | 7 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 84%](bars/features-temple.svg?v=a2add9f2) | 73 | 0 | 17 (11.5 done) | 0 | 11 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 82%](bars/features-miracles.svg?v=00ca9394) | 512 | 0 | 209 (149.0 done) | 12 | 70 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 81%](bars/features-hand.svg?v=e7efda09) | 489 | 0 | 101 (61.4 done) | 21 | 66 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 76%](bars/features-interface.svg?v=1efdf16a) | 211 | 2 | 64 (36.3 done) | 18 | 35 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](bars/features-camera.svg?v=35c561dc) | 119 | 0 | 62 (35.5 done) | 3 | 21 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Multiplayer](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 38%](bars/features-multiplayer.svg?v=5b0a94e0) | 123 | 0 | 69 | 0 | 135 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:multiplayer) |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](bars/features-scripts.svg?v=ea166747) | 1240 | 0 | 198 (93.9 done) | 1 | 165 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟨⬜⬜⬜⬜⬜⬜⬜ 33%](bars/features-rival-gods.svg?v=f423b6b8) | 98 | 0 | 88 (38.4 done) | 0 | 233 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rival_gods) |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 17%](bars/features-easter-eggs.svg?v=412e05c9) | 6 | 0 | 10 (5.9 done) | 0 | 56 |  |
| [Story](progress/INDEX.md#story) | ![🟨⬜⬜⬜⬜⬜⬜⬜⬜⬜ 12%](bars/features-story.svg?v=b6cf2efa) | 223 | 0 | 754 (329.1 done) | 0 | 3514 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story) |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-audio.svg?v=a8e08bd7) | 106 | 0 | 3 (1.7 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-video.svg?v=a879059f) | 79 | 0 | 2 (1.2 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [Debug tools](progress/INDEX.md#debug) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](bars/features-debug.svg?v=79926541) | 165 | 0 | 31 (18.2 done) | 0 | 27 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:debug) |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 73%](bars/features-rendering.svg?v=5d83c81b) | 113 | 1 | 33 (18.8 done) | 1 | 35 | [13 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 67%](bars/features-engine.svg?v=0af15117) | 119 | 0 | 91 (53.1 done) | 0 | 46 | [10 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 36%](bars/features-pc-integration.svg?v=b1bc2be3) | 43 | 0 | 6 (2.3 done) | 2 | 75 |  |

🐞 [Vanilla bugs](progress/vanilla_bugs/README.md): 242 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`e78437a5`, 2026-10-24), checked against our tree.

![🟩🟩⬜⬜⬜⬜⬜⬜⬜⬜ 21%](bars/raffclar-all.svg?v=5dfc5007)

**132** in · **60** partial · **591** pending · **38** n/a, not taken: his inspector and MCP adapter, parked by decision C-1 (21); CI and build tooling (10); his own docs pages (5); formatting (1); reverts (1).

The list, pull request by pull request: [raffclar-status.md](raffclar-status.md).

<a id="refactor"></a>

## 🧹 Code clean-up

How far the code follows the [code conventions](refactor-conventions.md#progress-counts); the notes on each goal are there.

🟩 done · 🟧 left on purpose · ⬜ pending

| Goal | Progress |
|---|---|
| [Mutable globals and file- or function-scope statics in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~96%](bars/refactor-mutable-globals-and-file-or-function.svg?v=6139d414) |
| [`Game::Instance()` call sites](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-game-instance-call-sites.svg?v=07084c46) |
| [Plain `enum` declarations](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-plain-enum-declarations.svg?v=07084c46) |
| [`new` / `delete` in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-new-delete-in-game-code.svg?v=07084c46) |
| [C arrays](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-c-arrays.svg?v=07084c46) |
| [Direct file reads (`std::ifstream`, `fopen`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~60%](bars/refactor-direct-file-reads-std-ifstream-fopen.svg?v=ffebe33c) |
| [Comments naming the original binary (`fn_…`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-naming-the-original-binary-fn.svg?v=07084c46) |
| [Comments quoting the original's addresses](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-quoting-the-originals-addresses.svg?v=07084c46) |
| [Identifiers named after the original binary or unclear](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~95%](bars/refactor-identifiers-named-after-the-original.svg?v=e87ec08c) |
| [Files that fail the CI's clang-format 17 check](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 81; Test files 151 → 827.

<a id="milestones"></a>

## 🏁 Milestones

- **2026-10-22** · [The checks read the original's meshes](milestones.md#the-checks-read-the-originals-meshes)
- **2026-10-22** · [Mods: the SDK's version, the Mods page and data mods](milestones.md#mods-the-sdks-version-the-mods-page-and-data-mods)
- **2026-10-21** · [Faster builds](milestones.md#faster-builds)
- **2026-10-21** · [The leashes a creature knows in the state hash](milestones.md#the-leashes-a-creature-knows-in-the-state-hash)
- **2026-10-21** · [The top detail level draws the high meshes, chosen from Maximum Detail](milestones.md#the-top-detail-level-and-maximum-detail)
- **2026-10-20** · [Recreate the original, on raffclar's architecture](milestones.md#recreate-the-original-on-raffclars-architecture)

[All milestones](milestones.md).

<!-- /dashboard:static -->

## How the numbers are counted

- **Features of the original**: every behaviour of the original game is one row of the
  [progress files](progress/README.md), marked done, chosen, partial NN%, inferred or pending; behaviour the original
  does not have has no row ([Not in the original](not-in-the-original.md)). A score counts done and chosen as 1, a
  partial row as its share (a partial row not measured yet as 0), inferred and pending as 0 (user, 2026-10-21). A
  domain with no rows is not shown.
- **raffclar's work**: one count of the pieces of his work, each counted once, in or not in our tree:

  *raffclar's work = (closed port rows + newer pull requests in) / (all port rows + all newer pull requests that add
  features)*

  - *Part 1, his target build*: the rows of the [port audit](port-audit.md), which compared raffclar's `master` at
    `0222f9f5` with our tree. A row closes when our tree has the piece, in his layout, verified.
  - *Part 2, his newer pull requests*: those merged after that build whose gain to our feature rows, estimated in
    [raffclar_uplift.tsv](raffclar_uplift.tsv) (done 1, partial half), is above zero. One is in when a commit of ours
    says "Ported from raffclar/openblack PR #N". The rows of gain the ported ones carry are shown beside the count.
  - *Why no weights*: a port row has no estimated gain and a pull request is not split into port rows, so the two
    parts share no measure of size. Weighting either one would mean inventing numbers, so each piece counts as one.
    Pull requests with no feature gain (fixes, tools) and the ones already inside the target build are left out, so
    nothing is counted twice.
- **Refactor**: the counts of the [code conventions](refactor-conventions.md#progress-counts). A measure below 100%
  says what is left, and whether that part stays on purpose (an accepted exception).
- **Bars**: each bar is a small SVG file in [bars/](bars/), written by `docs/tools/progress_stats.py` and rewritten
  only when its numbers change: green done, striped yellow partial, grey pending, striped orange left on purpose,
  with the percentage inside. Its text form (the alt text, shown where images are not) is 10 squares, each a tenth of
  its total, rounded: 🟩 done, 🟨 partial, ⬜ pending, 🟧 left on purpose.
- **Families**: the domains are grouped into World, Beings, Player and interface, Story and Engine; a family's line
  sums its domains' rows. "Biggest gains left" names the three domains with the most pending rows.

Each name links to its page; "N open" opens the fork's open issues for that area. In the viewer (`docs/index.html`)
the bars are counted live from those pages; on GitHub the copy above is written by `docs/tools/progress_stats.py`
whenever the pages change.
