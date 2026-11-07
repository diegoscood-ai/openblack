# openblack wiki

How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.

- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse [the pages](SUMMARY.md) here.
- **Start:** the [progress dashboard](wiki/project/dashboard.md) and [Reading this wiki](wiki/project/reading-the-wiki.md).

*This page is written by `docs/tools/progress_stats.py`.*


<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](wiki/project/bars/features-all.svg?v=399e84dc)

9772 of the original game's 12760 behaviours are done (6295 with a test, 394 proven in play, 135 of them both, 3218 with neither) and 7 done as the user chose; 1912 partial (9% of the rows' worth, 13 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 196 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 45%](wiki/project/bars/raffclar-all.svg?v=af57679b)

190 of the 665 pull requests we take from him are in our tree, 219 more in part (230 we have our own version of and 209 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](wiki/project/bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](wiki/project/progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](wiki/project/progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/family-world.svg?v=701815ba) | 946 | 1 | 86 | 2 | 97 (67.2 done) | 0 | 37 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](wiki/project/bars/family-creature.svg?v=f3b8251e) | 925 | 2 | 13 | 0 | 386 (227.0 done) | 0 | 159 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](wiki/project/bars/family-villagers-towns-and-animals.svg?v=ae0a428d) | 745 | 4 | 31 | 1 | 141 (90.9 done) | 0 | 73 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/family-player-and-interface.svg?v=02c6524c) | 1618 | 132 | 196 | 3 | 331 (220.3 done) | 0 | 106 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](wiki/project/bars/family-story.svg?v=084d7801) | 1608 | 254 | 2872 | 0 | 813 (487.0 done) | 0 | 583 |
| 💾 Save and load | ![🟩🟨🟨⬜⬜⬜⬜⬜⬜⬜ 26%](wiki/project/bars/family-save-and-load.svg?v=4ce78220) | 13 | 0 | 1 | 0 | 44 (20.4 done) | 0 | 73 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](wiki/project/bars/family-engine.svg?v=2c7672d2) | 440 | 1 | 19 | 1 | 100 (63.8 done) | 0 | 38 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 38%](wiki/project/bars/family-online-parked.svg?v=502efc48) | 29 | 0 | 44 | 0 | 8 (0.5 done) | 1 | 114 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](wiki/project/progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](wiki/project/bars/story-land-1.svg?v=9c35709f) | 1156 | 84% of 529 | 85% of 555 | 58% of 72 |
| [Land 2](wiki/project/progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](wiki/project/bars/story-land-2.svg?v=cc1c6140) | 1258 | 91% of 518 | 86% of 676 | 69% of 64 |
| [Land 3](wiki/project/progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 82%](wiki/project/bars/story-land-3.svg?v=38c76347) | 326 | 81% of 178 | 91% of 115 | 54% of 33 |
| [Land 4](wiki/project/progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](wiki/project/bars/story-land-4.svg?v=e914af7d) | 446 | 86% of 301 | 88% of 99 | 60% of 46 |
| [Land 5](wiki/project/progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](wiki/project/bars/story-land-5.svg?v=6178e69b) | 542 | 80% of 212 | 88% of 299 | 50% of 31 |
| [Tutorial island](wiki/project/progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 77%](wiki/project/bars/story-tutorial-island.svg?v=577b2087) | 52 | 79% of 51 | — | 0% of 1 |
| [Across the lands](wiki/project/progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 77%](wiki/project/bars/story-across-the-lands.svg?v=fccf058e) | 526 | — | 81% of 111 | 76% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](wiki/project/progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-ocean.svg?v=f11f245a) | 37 | 0 | 22 | 0 | 0 | 0 | 0 |  |
| [Sky](wiki/project/progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](wiki/project/progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-weather.svg?v=e5a1a714) | 116 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Terrain](wiki/project/progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](wiki/project/bars/features-terrain.svg?v=599b9d33) | 91 | 0 | 19 | 0 | 4 (3.0 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Physics](wiki/project/progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](wiki/project/bars/features-physics.svg?v=02bc7ffc) | 273 | 0 | 26 | 0 | 37 (26.3 done) | 0 | 6 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics,area:particles) |
| [Food, wood and resources](wiki/project/progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/features-resources.svg?v=e286fef7) | 92 | 0 | 0 | 0 | 14 (10.3 done) | 0 | 8 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:resources) |
| [Nature](wiki/project/progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 85%](wiki/project/bars/features-nature.svg?v=d1d3f11d) | 155 | 1 | 17 | 0 | 40 (25.8 done) | 0 | 22 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:nature) |
| [Creature](wiki/project/progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](wiki/project/bars/features-creature.svg?v=f3b8251e) | 925 | 2 | 13 | 0 | 386 (227.0 done) | 0 | 159 | [26 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Worship and belief](wiki/project/progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](wiki/project/bars/features-worship.svg?v=b6e34350) | 109 | 0 | 0 | 0 | 8 (4.7 done) | 0 | 7 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Towns](wiki/project/progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 90%](wiki/project/bars/features-town.svg?v=6a0a7409) | 132 | 0 | 0 | 1 | 55 (39.0 done) | 0 | 3 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Animals](wiki/project/progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 89%](wiki/project/bars/features-animal.svg?v=723639ce) | 65 | 0 | 0 | 0 | 18 (12.9 done) | 0 | 5 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Villagers](wiki/project/progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](wiki/project/bars/features-villager.svg?v=457eca8d) | 268 | 2 | 29 | 0 | 28 (15.5 done) | 0 | 38 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Buildings](wiki/project/progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](wiki/project/bars/features-building.svg?v=d86819ba) | 171 | 2 | 2 | 0 | 32 (18.8 done) | 0 | 20 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Gestures](wiki/project/progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/features-gesture.svg?v=69b557dc) | 107 | 2 | 0 | 0 | 2 (1.8 done) | 0 | 7 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [The hand](wiki/project/progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](wiki/project/bars/features-hand.svg?v=71166b38) | 548 | 91 | 3 | 0 | 64 (37.5 done) | 0 | 22 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Temple](wiki/project/progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](wiki/project/bars/features-temple.svg?v=9681f679) | 77 | 0 | 1 | 0 | 11 (7.0 done) | 0 | 4 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Interface](wiki/project/progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/features-interface.svg?v=2ece9b0e) | 241 | 16 | 1 | 3 | 36 (22.1 done) | 0 | 15 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Miracles](wiki/project/progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/features-miracles.svg?v=a1ee2502) | 473 | 1 | 127 | 0 | 137 (97.2 done) | 0 | 41 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Camera](wiki/project/progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 89%](wiki/project/bars/features-camera.svg?v=ba360261) | 150 | 3 | 0 | 0 | 42 (27.2 done) | 0 | 7 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Skirmish and multiplayer lands](wiki/project/progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](wiki/project/bars/features-multiplayer.svg?v=6fb9f92c) | 22 | 19 | 64 | 0 | 39 (27.5 done) | 0 | 10 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:multiplayer) |
| [Land 1](wiki/project/progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](wiki/project/bars/story-land-1.svg?v=9c35709f) | 33 | 92 | 734 | 0 | 174 (99.6 done) | 0 | 124 |  |
| [Land 2](wiki/project/progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](wiki/project/bars/story-land-2.svg?v=cc1c6140) | 23 | 93 | 924 | 0 | 99 (59.8 done) | 0 | 120 |  |
| [Land 3](wiki/project/progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 82%](wiki/project/bars/story-land-3.svg?v=38c76347) | 3 | 3 | 248 | 0 | 22 (13.0 done) | 0 | 50 |  |
| [Land 4](wiki/project/progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](wiki/project/bars/story-land-4.svg?v=e914af7d) | 5 | 4 | 330 | 0 | 58 (35.3 done) | 0 | 50 |  |
| [Land 5](wiki/project/progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](wiki/project/bars/story-land-5.svg?v=6178e69b) | 6 | 2 | 405 | 0 | 57 (34.4 done) | 0 | 72 |  |
| [Tutorial island](wiki/project/progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 77%](wiki/project/bars/story-tutorial-island.svg?v=577b2087) | 5 | 20 | 12 | 0 | 15 (6.3 done) | 0 | 3 |  |
| [Across the lands](wiki/project/progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 77%](wiki/project/bars/story-across-the-lands.svg?v=fccf058e) | 207 | 8 | 124 | 0 | 108 (68.8 done) | 0 | 82 |  |
| [Scripts](wiki/project/progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](wiki/project/bars/features-scripts.svg?v=b48ac06a) | 1203 | 14 | 20 | 0 | 127 (75.7 done) | 0 | 17 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Easter eggs](wiki/project/progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 71%](wiki/project/bars/features-easter-eggs.svg?v=dff096a2) | 18 | 12 | 9 | 0 | 11 (6.8 done) | 0 | 14 |  |
| [Rival gods](wiki/project/progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 71%](wiki/project/bars/features-rival-gods.svg?v=69d5c07b) | 105 | 6 | 66 | 0 | 142 (87.2 done) | 0 | 51 |  |
| [💾 Loading a save](wiki/project/progress/engine/saving_and_loading.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 34%](wiki/project/bars/save-loading.svg?v=6874c72d) | 0 | 0 | 0 | 0 | 6 (3.1 done) | 0 | 3 |  |
| [💾 Writing a save](wiki/project/progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 57%](wiki/project/bars/save-saving.svg?v=4e776d01) | 5 | 0 | 0 | 0 | 10 (5.2 done) | 0 | 3 |  |
| [💾 Kept in the save (one row for both halves)](wiki/project/progress/engine/saving_and_loading.md) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 20%](wiki/project/bars/save-both.svg?v=e53bd089) | 8 | 0 | 1 | 0 | 28 (12.1 done) | 0 | 67 |  |
| [Sound and music](wiki/project/progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-audio.svg?v=81feba96) | 106 | 1 | 1 | 0 | 1 (0.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](wiki/project/progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/features-video.svg?v=6f52d4b1) | 55 | 0 | 1 | 0 | 2 (1.2 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [PC integration](wiki/project/progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 85%](wiki/project/bars/features-pc-integration.svg?v=6b0dee13) | 50 | 0 | 3 | 0 | 3 (2.0 done) | 0 | 9 |  |
| [Rendering](wiki/project/progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 84%](wiki/project/bars/features-rendering.svg?v=c27a4bae) | 123 | 0 | 7 | 1 | 35 (21.0 done) | 0 | 16 | [16 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [Engine and files](wiki/project/progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 83%](wiki/project/bars/features-engine.svg?v=be1e98c7) | 106 | 0 | 7 | 0 | 59 (39.0 done) | 0 | 12 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [⏸️ Launch switches and stray files](wiki/project/progress/easter_eggs/launch_switches_and_files.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-easter-eggs-launch-switches-and-files.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Firestorm](wiki/project/progress/multiplayer/maps/firestorm.md) | ![🟩🟩🟩🟩🟩🟩🟩⬜⬜⬜ 69%](wiki/project/bars/online-multiplayer-maps-firestorm.svg?v=0a06009e) | 0 | 0 | 20 | 0 | 6 | 0 | 3 |  |
| [⏸️ Four Gods](wiki/project/progress/multiplayer/maps/four_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-four-gods.svg?v=e41d6658) | 0 | 0 | 0 | 0 | 0 | 0 | 6 |  |
| [⏸️ Three Gods](wiki/project/progress/multiplayer/maps/three_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-three-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Two Gods](wiki/project/progress/multiplayer/maps/two_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-two-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Multiplayer rules](wiki/project/progress/multiplayer/multiplayer_rules.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-multiplayer-rules.svg?v=8bb63ed6) | 0 | 0 | 0 | 0 | 0 | 0 | 21 |  |
| [⏸️ Network play](wiki/project/progress/multiplayer/network_play.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-network-play.svg?v=99990afc) | 0 | 0 | 0 | 0 | 1 | 0 | 18 |  |
| [⏸️ Online services](wiki/project/progress/multiplayer/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-online-services.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Skirmish](wiki/project/progress/multiplayer/skirmish.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-skirmish.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Online services and PC extras](wiki/project/progress/pc_integration/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-pc-integration-online-services.svg?v=efe8a375) | 0 | 0 | 0 | 0 | 0 | 0 | 9 |  |
| [⏸️ Villagers named after the player's contacts](wiki/project/progress/pc_integration/villager_names_from_contacts.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-pc-integration-villager-names-from.svg?v=579d3a06) | 0 | 0 | 0 | 0 | 0 | 1 | 38 |  |
| [⏸️ Playground scripts](wiki/project/progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/online-scripts-playground-scripts.svg?v=2d2cd500) | 29 | 0 | 24 | 0 | 1 (0.5 done) | 0 | 0 |  |

🐞 [Vanilla bugs](wiki/project/progress/vanilla_bugs/README.md): 241 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`55aec140`, 2026-11-06), checked against our tree.

![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 45%](wiki/project/bars/raffclar-all.svg?v=af57679b)

**420** in (190 ported, 230 our own version) · **219** partial · **256** pending · **209** n/a, not taken: CI and build tooling (77); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (34); his deviations from the original, not chosen by us (9); his own docs pages (8); developer tools and behaviour the original does not have (7); reverts (2); formatting (1); force-feedback device behaviour, not ported (user, 2026-10-27) (1).

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
| [Identifiers named after the original binary or unclear](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~99%](wiki/project/bars/refactor-identifiers-named-after-the-original.svg?v=6aafed7d) |
| [Files that fail the CI's clang-format 17 check](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 986.

<a id="milestones"></a>

## 🏁 Milestones

- **2026-10-22** · [The checks read the original's meshes](wiki/project/milestones.md#the-checks-read-the-originals-meshes)
- **2026-10-22** · [Mods: the SDK's version, the Mods page and data mods](wiki/project/milestones.md#mods-the-sdks-version-the-mods-page-and-data-mods)
- **2026-10-21** · [Faster builds](wiki/project/milestones.md#faster-builds)
- **2026-10-21** · [The leashes a creature knows in the state hash](wiki/project/milestones.md#the-leashes-a-creature-knows-in-the-state-hash)
- **2026-10-21** · [The top detail level draws the high meshes, chosen from Maximum Detail](wiki/project/milestones.md#the-top-detail-level-and-maximum-detail)
- **2026-10-20** · [Recreate the original, on raffclar's architecture](wiki/project/milestones.md#recreate-the-original-on-raffclars-architecture)

[All milestones](wiki/project/milestones.md).
