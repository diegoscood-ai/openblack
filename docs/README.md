# openblack wiki

How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.

- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse [the pages](SUMMARY.md) here.
- **Start:** the [progress dashboard](wiki/project/dashboard.md) (live bars in the viewer) and [Reading this wiki](wiki/project/reading-the-wiki.md).

*This page is written by `tools/progress_stats.py`.*


<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟨🟪⬜⬜⬜⬜ 55%](wiki/project/bars/features-all.svg?v=74a05c4d)

6279 of the original game's 13975 behaviours are done and 5 done as the user chose; 2504 partial (10% of the rows' worth, 237 not measured yet); 240 inferred, not yet checked against the original.

**🔀 raffclar's work in our tree**

![🟩🟩⬜⬜⬜⬜⬜⬜⬜⬜ 21%](wiki/project/bars/raffclar-all.svg?v=5dfc5007)

132 of his 783 pull requests are in our tree, 60 more in part.

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](wiki/project/bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](wiki/project/progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing.

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 89%](wiki/project/bars/family-world.svg?v=a0b7f50e) | 944 | 2 | 166 (123.2 done) | 24 | 69 |
| 🐾 Beings | ![🟩🟩🟩🟩🟩🟩🟨🟪⬜⬜ 71%](wiki/project/bars/family-beings.svg?v=e184da68) | 1509 | 0 | 598 (372.4 done) | 158 | 380 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](wiki/project/bars/family-player-and-interface.svg?v=ad94b095) | 1634 | 2 | 524 (295.6 done) | 54 | 345 |
| 📜 Story | ![🟩🟩🟨⬜⬜⬜⬜⬜⬜⬜ 31%](wiki/project/bars/family-story.svg?v=954a115b) | 1567 | 0 | 1050 (467.3 done) | 1 | 3968 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 74%](wiki/project/bars/family-engine.svg?v=bea2aed5) | 625 | 1 | 166 (95.3 done) | 3 | 185 |

<details><summary>Every domain</summary>

| Domain | Progress | Done | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|
| [Ocean](wiki/project/progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-ocean.svg?v=f11f245a) | 59 | 0 | 0 | 0 | 0 |  |
| [Sky](wiki/project/progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-sky.svg?v=67e1023d) | 181 | 2 | 2 (1.9 done) | 0 | 2 |  |
| [Terrain](wiki/project/progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/features-terrain.svg?v=76ef8f39) | 109 | 0 | 7 (5.2 done) | 0 | 2 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Weather](wiki/project/progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/features-weather.svg?v=023fe5ee) | 115 | 0 | 3 (0.8 done) | 0 | 5 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:weather) |
| [Physics](wiki/project/progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](wiki/project/bars/features-physics.svg?v=36f42d0d) | 269 | 0 | 69 (50.9 done) | 5 | 9 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics) |
| [Food, wood and resources](wiki/project/progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](wiki/project/bars/features-resources.svg?v=23d76bb8) | 76 | 0 | 29 (21.9 done) | 3 | 16 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:resources) |
| [Nature](wiki/project/progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟨🟪⬜⬜ 73%](wiki/project/bars/features-nature.svg?v=0015d9a8) | 135 | 0 | 56 (42.6 done) | 16 | 35 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:nature) |
| [Animals](wiki/project/progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟨🟨🟪⬜ 84%](wiki/project/bars/features-animal.svg?v=0b76b69f) | 54 | 0 | 27 (21.5 done) | 3 | 6 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Worship and belief](wiki/project/progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](wiki/project/bars/features-worship.svg?v=f3421b18) | 101 | 0 | 24 (16.7 done) | 2 | 16 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Buildings](wiki/project/progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 80%](wiki/project/bars/features-building.svg?v=1a800853) | 154 | 0 | 69 (47.0 done) | 3 | 26 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Villagers](wiki/project/progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 79%](wiki/project/bars/features-villager.svg?v=092e5d59) | 270 | 0 | 33 (18.0 done) | 5 | 57 | [12 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Towns](wiki/project/progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](wiki/project/bars/features-town.svg?v=2d91029b) | 120 | 0 | 49 (33.9 done) | 2 | 34 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Creature](wiki/project/progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 66%](wiki/project/bars/features-creature.svg?v=c0da3c8b) | 810 | 0 | 396 (235.3 done) | 143 | 241 | [24 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Gestures](wiki/project/progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/features-gesture.svg?v=69b557dc) | 107 | 0 | 2 (1.8 done) | 0 | 7 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [Temple](wiki/project/progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 84%](wiki/project/bars/features-temple.svg?v=a2add9f2) | 73 | 0 | 17 (11.5 done) | 0 | 11 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Miracles](wiki/project/progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 82%](wiki/project/bars/features-miracles.svg?v=00ca9394) | 512 | 0 | 209 (149.0 done) | 12 | 70 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [The hand](wiki/project/progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 81%](wiki/project/bars/features-hand.svg?v=e7efda09) | 489 | 0 | 101 (61.4 done) | 21 | 66 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Interface](wiki/project/progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 76%](wiki/project/bars/features-interface.svg?v=1efdf16a) | 211 | 2 | 64 (36.3 done) | 18 | 35 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Camera](wiki/project/progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 75%](wiki/project/bars/features-camera.svg?v=35c561dc) | 119 | 0 | 62 (35.5 done) | 3 | 21 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Multiplayer](wiki/project/progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 38%](wiki/project/bars/features-multiplayer.svg?v=5b0a94e0) | 123 | 0 | 69 | 0 | 135 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:multiplayer) |
| [Scripts](wiki/project/progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](wiki/project/bars/features-scripts.svg?v=ea166747) | 1240 | 0 | 198 (93.9 done) | 1 | 165 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Rival gods](wiki/project/progress/INDEX.md#rival_gods) | ![🟩🟩🟨⬜⬜⬜⬜⬜⬜⬜ 33%](wiki/project/bars/features-rival-gods.svg?v=f423b6b8) | 98 | 0 | 88 (38.4 done) | 0 | 233 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rival_gods) |
| [Easter eggs](wiki/project/progress/INDEX.md#easter_eggs) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 17%](wiki/project/bars/features-easter-eggs.svg?v=412e05c9) | 6 | 0 | 10 (5.9 done) | 0 | 56 |  |
| [Story](wiki/project/progress/INDEX.md#story) | ![🟨⬜⬜⬜⬜⬜⬜⬜⬜⬜ 12%](wiki/project/bars/features-story.svg?v=b6cf2efa) | 223 | 0 | 754 (329.1 done) | 0 | 3514 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story) |
| [Sound and music](wiki/project/progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](wiki/project/bars/features-audio.svg?v=a8e08bd7) | 106 | 0 | 3 (1.7 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](wiki/project/progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](wiki/project/bars/features-video.svg?v=a879059f) | 79 | 0 | 2 (1.2 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [Debug tools](wiki/project/progress/INDEX.md#debug) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](wiki/project/bars/features-debug.svg?v=79926541) | 165 | 0 | 31 (18.2 done) | 0 | 27 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:debug) |
| [Rendering](wiki/project/progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 73%](wiki/project/bars/features-rendering.svg?v=5d83c81b) | 113 | 1 | 33 (18.8 done) | 1 | 35 | [13 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [Engine and files](wiki/project/progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 67%](wiki/project/bars/features-engine.svg?v=0af15117) | 119 | 0 | 91 (53.1 done) | 0 | 46 | [10 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [PC integration](wiki/project/progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 36%](wiki/project/bars/features-pc-integration.svg?v=b1bc2be3) | 43 | 0 | 6 (2.3 done) | 2 | 75 |  |

🐞 [Vanilla bugs](wiki/project/progress/vanilla_bugs/README.md): 242 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`e78437a5`, 2026-10-24), checked against our tree.

![🟩🟩⬜⬜⬜⬜⬜⬜⬜⬜ 21%](wiki/project/bars/raffclar-all.svg?v=5dfc5007)

**132** in · **60** partial · **591** pending · **38** n/a, not taken: his inspector and MCP adapter, parked by decision C-1 (21); CI and build tooling (10); his own docs pages (5); formatting (1); reverts (1).

The list, pull request by pull request: [raffclar-status.md](wiki/project/raffclar-status.md).

<a id="refactor"></a>

## 🧹 Code clean-up

How far the code follows the [code conventions](wiki/project/refactor-conventions.md#progress-counts); the notes on each goal are there.

🟩 done · 🟧 left on purpose · ⬜ pending

| Goal | Progress |
|---|---|
| [Mutable globals and file- or function-scope statics in game code](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~96%](wiki/project/bars/refactor-mutable-globals-and-file-or-function.svg?v=6139d414) |
| [`Game::Instance()` call sites](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-game-instance-call-sites.svg?v=07084c46) |
| [Plain `enum` declarations](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-plain-enum-declarations.svg?v=07084c46) |
| [`new` / `delete` in game code](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-new-delete-in-game-code.svg?v=07084c46) |
| [C arrays](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-c-arrays.svg?v=07084c46) |
| [Direct file reads (`std::ifstream`, `fopen`)](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~60%](wiki/project/bars/refactor-direct-file-reads-std-ifstream-fopen.svg?v=ffebe33c) |
| [Comments naming the original binary (`fn_…`)](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-comments-naming-the-original-binary-fn.svg?v=07084c46) |
| [Comments quoting the original's addresses](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-comments-quoting-the-originals-addresses.svg?v=07084c46) |
| [Identifiers named after the original binary or unclear](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~95%](wiki/project/bars/refactor-identifiers-named-after-the-original.svg?v=e87ec08c) |
| [Files that fail the CI's clang-format 17 check](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 81; Test files 151 → 827.

<a id="milestones"></a>

## 🏁 Milestones

- **2026-10-22** · [The checks read the original's meshes](wiki/project/milestones.md#the-checks-read-the-originals-meshes)
- **2026-10-22** · [Mods: the SDK's version, the Mods page and data mods](wiki/project/milestones.md#mods-the-sdks-version-the-mods-page-and-data-mods)
- **2026-10-21** · [Faster builds](wiki/project/milestones.md#faster-builds)
- **2026-10-21** · [The leashes a creature knows in the state hash](wiki/project/milestones.md#the-leashes-a-creature-knows-in-the-state-hash)
- **2026-10-21** · [The top detail level draws the high meshes, chosen from Maximum Detail](wiki/project/milestones.md#the-top-detail-level-and-maximum-detail)
- **2026-10-20** · [Recreate the original, on raffclar's architecture](wiki/project/milestones.md#recreate-the-original-on-raffclars-architecture)

[All milestones](wiki/project/milestones.md).
