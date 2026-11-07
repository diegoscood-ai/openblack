# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/features-all.svg?v=399e84dc)

9772 of the original game's 12760 behaviours are done (6295 with a test, 394 proven in play, 135 of them both, 3218 with neither) and 7 done as the user chose; 1912 partial (9% of the rows' worth, 13 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 196 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 45%](bars/raffclar-all.svg?v=af57679b)

190 of the 665 pull requests we take from him are in our tree, 219 more in part (230 we have our own version of and 209 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/family-world.svg?v=701815ba) | 946 | 1 | 86 | 2 | 97 (67.2 done) | 0 | 37 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](bars/family-creature.svg?v=f3b8251e) | 925 | 2 | 13 | 0 | 386 (227.0 done) | 0 | 159 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/family-villagers-towns-and-animals.svg?v=ae0a428d) | 745 | 4 | 31 | 1 | 141 (90.9 done) | 0 | 73 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/family-player-and-interface.svg?v=02c6524c) | 1618 | 132 | 196 | 3 | 331 (220.3 done) | 0 | 106 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](bars/family-story.svg?v=084d7801) | 1608 | 254 | 2872 | 0 | 813 (487.0 done) | 0 | 583 |
| 💾 Save and load | ![🟩🟨🟨⬜⬜⬜⬜⬜⬜⬜ 26%](bars/family-save-and-load.svg?v=4ce78220) | 13 | 0 | 1 | 0 | 44 (20.4 done) | 0 | 73 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/family-engine.svg?v=2c7672d2) | 440 | 1 | 19 | 1 | 100 (63.8 done) | 0 | 38 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 38%](bars/family-online-parked.svg?v=502efc48) | 29 | 0 | 44 | 0 | 8 (0.5 done) | 1 | 114 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/story-land-1.svg?v=9c35709f) | 1156 | 84% of 529 | 85% of 555 | 58% of 72 |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-land-2.svg?v=cc1c6140) | 1258 | 91% of 518 | 86% of 676 | 69% of 64 |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 82%](bars/story-land-3.svg?v=38c76347) | 326 | 81% of 178 | 91% of 115 | 54% of 33 |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-4.svg?v=e914af7d) | 446 | 86% of 301 | 88% of 99 | 60% of 46 |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](bars/story-land-5.svg?v=6178e69b) | 542 | 80% of 212 | 88% of 299 | 50% of 31 |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 77%](bars/story-tutorial-island.svg?v=577b2087) | 52 | 79% of 51 | — | 0% of 1 |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 77%](bars/story-across-the-lands.svg?v=fccf058e) | 526 | — | 81% of 111 | 76% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 37 | 0 | 22 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-weather.svg?v=e5a1a714) | 116 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-terrain.svg?v=599b9d33) | 91 | 0 | 19 | 0 | 4 (3.0 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/features-physics.svg?v=02bc7ffc) | 273 | 0 | 26 | 0 | 37 (26.3 done) | 0 | 6 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics,area:particles) |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-resources.svg?v=e286fef7) | 92 | 0 | 0 | 0 | 14 (10.3 done) | 0 | 8 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:resources) |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 85%](bars/features-nature.svg?v=d1d3f11d) | 155 | 1 | 17 | 0 | 40 (25.8 done) | 0 | 22 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:nature) |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 79%](bars/features-creature.svg?v=f3b8251e) | 925 | 2 | 13 | 0 | 386 (227.0 done) | 0 | 159 | [26 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](bars/features-worship.svg?v=b6e34350) | 109 | 0 | 0 | 0 | 8 (4.7 done) | 0 | 7 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 90%](bars/features-town.svg?v=6a0a7409) | 132 | 0 | 0 | 1 | 55 (39.0 done) | 0 | 3 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 89%](bars/features-animal.svg?v=723639ce) | 65 | 0 | 0 | 0 | 18 (12.9 done) | 0 | 5 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/features-villager.svg?v=457eca8d) | 268 | 2 | 29 | 0 | 28 (15.5 done) | 0 | 38 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](bars/features-building.svg?v=d86819ba) | 171 | 2 | 2 | 0 | 32 (18.8 done) | 0 | 20 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/features-gesture.svg?v=69b557dc) | 107 | 2 | 0 | 0 | 2 (1.8 done) | 0 | 7 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](bars/features-hand.svg?v=71166b38) | 548 | 91 | 3 | 0 | 64 (37.5 done) | 0 | 22 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/features-temple.svg?v=9681f679) | 77 | 0 | 1 | 0 | 11 (7.0 done) | 0 | 4 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-interface.svg?v=2ece9b0e) | 241 | 16 | 1 | 3 | 36 (22.1 done) | 0 | 15 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-miracles.svg?v=a1ee2502) | 473 | 1 | 127 | 0 | 137 (97.2 done) | 0 | 41 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 89%](bars/features-camera.svg?v=ba360261) | 150 | 3 | 0 | 0 | 42 (27.2 done) | 0 | 7 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Skirmish and multiplayer lands](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](bars/features-multiplayer.svg?v=6fb9f92c) | 22 | 19 | 64 | 0 | 39 (27.5 done) | 0 | 10 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:multiplayer) |
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/story-land-1.svg?v=9c35709f) | 33 | 92 | 734 | 0 | 174 (99.6 done) | 0 | 124 |  |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-land-2.svg?v=cc1c6140) | 23 | 93 | 924 | 0 | 99 (59.8 done) | 0 | 120 |  |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 82%](bars/story-land-3.svg?v=38c76347) | 3 | 3 | 248 | 0 | 22 (13.0 done) | 0 | 50 |  |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-4.svg?v=e914af7d) | 5 | 4 | 330 | 0 | 58 (35.3 done) | 0 | 50 |  |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](bars/story-land-5.svg?v=6178e69b) | 6 | 2 | 405 | 0 | 57 (34.4 done) | 0 | 72 |  |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 77%](bars/story-tutorial-island.svg?v=577b2087) | 5 | 20 | 12 | 0 | 15 (6.3 done) | 0 | 3 |  |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 77%](bars/story-across-the-lands.svg?v=fccf058e) | 207 | 8 | 124 | 0 | 108 (68.8 done) | 0 | 82 |  |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/features-scripts.svg?v=b48ac06a) | 1203 | 14 | 20 | 0 | 127 (75.7 done) | 0 | 17 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 71%](bars/features-easter-eggs.svg?v=dff096a2) | 18 | 12 | 9 | 0 | 11 (6.8 done) | 0 | 14 |  |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 71%](bars/features-rival-gods.svg?v=69d5c07b) | 105 | 6 | 66 | 0 | 142 (87.2 done) | 0 | 51 |  |
| [💾 Loading a save](progress/engine/saving_and_loading.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 34%](bars/save-loading.svg?v=6874c72d) | 0 | 0 | 0 | 0 | 6 (3.1 done) | 0 | 3 |  |
| [💾 Writing a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 57%](bars/save-saving.svg?v=4e776d01) | 5 | 0 | 0 | 0 | 10 (5.2 done) | 0 | 3 |  |
| [💾 Kept in the save (one row for both halves)](progress/engine/saving_and_loading.md) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 20%](bars/save-both.svg?v=e53bd089) | 8 | 0 | 1 | 0 | 28 (12.1 done) | 0 | 67 |  |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-audio.svg?v=81feba96) | 106 | 1 | 1 | 0 | 1 (0.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-video.svg?v=6f52d4b1) | 55 | 0 | 1 | 0 | 2 (1.2 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 85%](bars/features-pc-integration.svg?v=6b0dee13) | 50 | 0 | 3 | 0 | 3 (2.0 done) | 0 | 9 |  |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 84%](bars/features-rendering.svg?v=c27a4bae) | 123 | 0 | 7 | 1 | 35 (21.0 done) | 0 | 16 | [16 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 83%](bars/features-engine.svg?v=be1e98c7) | 106 | 0 | 7 | 0 | 59 (39.0 done) | 0 | 12 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [⏸️ Launch switches and stray files](progress/easter_eggs/launch_switches_and_files.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-easter-eggs-launch-switches-and-files.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Firestorm](progress/multiplayer/maps/firestorm.md) | ![🟩🟩🟩🟩🟩🟩🟩⬜⬜⬜ 69%](bars/online-multiplayer-maps-firestorm.svg?v=0a06009e) | 0 | 0 | 20 | 0 | 6 | 0 | 3 |  |
| [⏸️ Four Gods](progress/multiplayer/maps/four_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-four-gods.svg?v=e41d6658) | 0 | 0 | 0 | 0 | 0 | 0 | 6 |  |
| [⏸️ Three Gods](progress/multiplayer/maps/three_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-three-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Two Gods](progress/multiplayer/maps/two_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-two-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Multiplayer rules](progress/multiplayer/multiplayer_rules.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-multiplayer-rules.svg?v=8bb63ed6) | 0 | 0 | 0 | 0 | 0 | 0 | 21 |  |
| [⏸️ Network play](progress/multiplayer/network_play.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-network-play.svg?v=99990afc) | 0 | 0 | 0 | 0 | 1 | 0 | 18 |  |
| [⏸️ Online services](progress/multiplayer/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-online-services.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Skirmish](progress/multiplayer/skirmish.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-skirmish.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Online services and PC extras](progress/pc_integration/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-online-services.svg?v=efe8a375) | 0 | 0 | 0 | 0 | 0 | 0 | 9 |  |
| [⏸️ Villagers named after the player's contacts](progress/pc_integration/villager_names_from_contacts.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-villager-names-from.svg?v=579d3a06) | 0 | 0 | 0 | 0 | 0 | 1 | 38 |  |
| [⏸️ Playground scripts](progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/online-scripts-playground-scripts.svg?v=2d2cd500) | 29 | 0 | 24 | 0 | 1 (0.5 done) | 0 | 0 |  |

🐞 [Vanilla bugs](progress/vanilla_bugs/README.md): 241 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`55aec140`, 2026-11-06), checked against our tree.

![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 45%](bars/raffclar-all.svg?v=af57679b)

**420** in (190 ported, 230 our own version) · **219** partial · **256** pending · **209** n/a, not taken: CI and build tooling (77); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (34); his deviations from the original, not chosen by us (9); his own docs pages (8); developer tools and behaviour the original does not have (7); reverts (2); formatting (1); force-feedback device behaviour, not ported (user, 2026-10-27) (1).

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
| [Identifiers named after the original binary or unclear](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~99%](bars/refactor-identifiers-named-after-the-original.svg?v=6aafed7d) |
| [Files that fail the CI's clang-format 17 check](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 986.

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
  row only a force-feedback device reaches (the hand's and the creature's touch effects, the mushroom puzzle) has the
  `device` status and is out of the score, like `n/a` (user, 2026-10-27: haptics are not ported). A domain with no
  rows is not shown.
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
- **Families**: the domains are grouped into World, Creature, Villagers, towns and animals, Player and interface,
  Story and Engine; a family's line sums its members' rows (user, 2026-10-24). Three families are made from rows, not
  whole domains, by the rules in [progress/README.md](progress/README.md#how-the-dashboard-groups-the-rows) (no row's
  status changes, only where it is counted, and each row is counted once):
  - *Save and load*: the [saving and loading](progress/engine/saving_and_loading.md) page and every row of another
    domain about the saved game, in three parts: loading a save, writing one, and what is kept in the save, one row
    for both halves. The domains they come from count the rest of their rows.
  - *Story, land by land*: the story domain's rows by land (Land 1 to 5, the tutorial island, across the lands), each
    land with its gold scrolls, silver scrolls and the rest apart.
  - *Online, parked* (user, 2026-10-24: online waits until the game works offline): network play, its rules and the
    online services, on a line of their own and in no score, the headline's included.
- **Done with a test**: a done row whose notes name a test of `test/`; the others are done without a test. Both are
  counted in every line; chosen rows are counted apart.
- **Playable by land**: not counted. The rows do not say which lands use which system, so a land's share of working
  systems cannot be read from them; the land lines count the story's rows only.

Each name links to its page; "N open" opens the fork's open issues for that area. The copy above is written by
`docs/tools/progress_stats.py` whenever the pages change (`tools/sync_docs.sh` runs it at every joint); the viewer
(`docs/index.html`) shows the same copy.
