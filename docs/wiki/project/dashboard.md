# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-all.svg?v=b21d065e)

10268 of the original game's 12689 behaviours are done (6652 with a test, 726 proven in play, 243 of them both, 3133 with neither) and 7 done as the user chose; 1729 partial (9% of the rows' worth, 13 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 196 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨🟨🟨⬜⬜ 54%](bars/raffclar-all.svg?v=67708809)

195 of the 635 pull requests we take from him are in our tree, 292 more in part (253 we have our own version of and 310 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/family-world.svg?v=71cd1388) | 960 | 1 | 86 | 2 | 89 (60.3 done) | 0 | 28 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/family-creature.svg?v=6610e692) | 1056 | 9 | 34 | 0 | 297 (178.8 done) | 0 | 57 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/family-villagers-towns-and-animals.svg?v=27233c3a) | 775 | 5 | 34 | 1 | 127 (79.5 done) | 0 | 50 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/family-player-and-interface.svg?v=4603b2ca) | 1722 | 194 | 195 | 3 | 285 (196.8 done) | 0 | 43 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/family-story.svg?v=edca7b7c) | 1640 | 482 | 2764 | 0 | 780 (496.6 done) | 0 | 460 |
| 💾 Save and load | ![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 59%](bars/family-save-and-load.svg?v=ff3117fe) | 41 | 24 | 1 | 0 | 63 (32.8 done) | 0 | 23 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/family-engine.svg?v=1b96d09d) | 458 | 11 | 19 | 1 | 88 (57.4 done) | 0 | 24 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 38%](bars/family-online-parked.svg?v=502efc48) | 29 | 0 | 44 | 0 | 8 (0.5 done) | 1 | 114 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-land-1.svg?v=ed862c45) | 1154 | 90% of 527 | 88% of 555 | 61% of 72 |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-land-2.svg?v=535f4719) | 1258 | 91% of 518 | 87% of 676 | 65% of 64 |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](bars/story-land-3.svg?v=f2613cd0) | 326 | 82% of 178 | 92% of 115 | 57% of 33 |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](bars/story-land-4.svg?v=3351fc76) | 446 | 87% of 301 | 90% of 99 | 62% of 46 |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-5.svg?v=a2c3794b) | 542 | 81% of 212 | 89% of 299 | 55% of 31 |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/story-tutorial-island.svg?v=fd9d9b01) | 52 | 85% of 51 | — | 0% of 1 |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 79%](bars/story-across-the-lands.svg?v=569410df) | 525 | — | 88% of 110 | 77% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 37 | 0 | 22 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-weather.svg?v=e5a1a714) | 116 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-terrain.svg?v=599b9d33) | 91 | 0 | 19 | 0 | 4 (3.0 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-physics.svg?v=4e84a35c) | 281 | 0 | 25 | 0 | 32 (23.1 done) | 0 | 3 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics) |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/features-resources.svg?v=eca8a394) | 96 | 0 | 0 | 0 | 12 (7.9 done) | 0 | 6 |  |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/features-nature.svg?v=5d688933) | 157 | 1 | 18 | 0 | 39 (24.6 done) | 0 | 18 |  |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/features-creature.svg?v=6610e692) | 1056 | 9 | 34 | 0 | 297 (178.8 done) | 0 | 57 | [19 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 94%](bars/features-animal.svg?v=722c468f) | 71 | 0 | 0 | 0 | 16 (11.1 done) | 0 | 0 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](bars/features-worship.svg?v=816efe05) | 111 | 0 | 0 | 0 | 8 (4.9 done) | 0 | 5 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 91%](bars/features-town.svg?v=054d853a) | 139 | 0 | 2 | 1 | 46 (32.4 done) | 0 | 3 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 89%](bars/features-building.svg?v=618d580f) | 182 | 2 | 2 | 0 | 26 (14.0 done) | 0 | 13 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/features-villager.svg?v=f570f2a4) | 272 | 3 | 30 | 0 | 31 (17.1 done) | 0 | 29 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-gesture.svg?v=eef5d61a) | 108 | 2 | 0 | 0 | 7 (4.8 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-camera.svg?v=62da4a31) | 172 | 23 | 0 | 0 | 25 (17.3 done) | 0 | 1 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-hand.svg?v=bb9a402b) | 575 | 109 | 3 | 0 | 54 (33.6 done) | 0 | 5 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-temple.svg?v=d4c58b95) | 81 | 3 | 1 | 0 | 12 (8.1 done) | 0 | 0 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/features-interface.svg?v=7ba584b0) | 265 | 37 | 1 | 3 | 17 (10.5 done) | 0 | 7 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 93%](bars/features-miracles.svg?v=2ceeff94) | 499 | 1 | 126 | 0 | 131 (94.8 done) | 0 | 19 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Skirmish and multiplayer lands](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](bars/features-multiplayer.svg?v=6fb9f92c) | 22 | 19 | 64 | 0 | 39 (27.5 done) | 0 | 10 |  |
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-land-1.svg?v=ed862c45) | 33 | 244 | 651 | 0 | 150 (84.6 done) | 0 | 82 |  |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-land-2.svg?v=535f4719) | 24 | 123 | 890 | 0 | 114 (68.8 done) | 0 | 108 |  |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 83%](bars/story-land-3.svg?v=f2613cd0) | 3 | 4 | 250 | 0 | 22 (13.0 done) | 0 | 47 |  |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 85%](bars/story-land-4.svg?v=3351fc76) | 5 | 7 | 339 | 0 | 49 (29.9 done) | 0 | 47 |  |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-5.svg?v=a2c3794b) | 8 | 8 | 414 | 0 | 44 (26.4 done) | 0 | 70 |  |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/story-tutorial-island.svg?v=fd9d9b01) | 5 | 25 | 11 | 0 | 11 (5.1 done) | 0 | 3 |  |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 79%](bars/story-across-the-lands.svg?v=569410df) | 210 | 17 | 128 | 0 | 105 (66.4 done) | 0 | 69 |  |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-scripts.svg?v=b5c73abc) | 1210 | 18 | 19 | 0 | 123 (78.2 done) | 0 | 8 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 89%](bars/features-easter-eggs.svg?v=ea7fbab7) | 23 | 15 | 9 | 0 | 16 (9.6 done) | 0 | 0 |  |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟨🟨🟨⬜⬜ 84%](bars/features-rival-gods.svg?v=3a547882) | 119 | 21 | 53 | 0 | 146 (114.6 done) | 0 | 26 |  |
| [💾 Loading a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟨🟨🟨⬜⬜⬜⬜⬜ 47%](bars/save-loading.svg?v=1fcc0cdc) | 1 | 2 | 0 | 0 | 4 (2.2 done) | 0 | 3 |  |
| [💾 Writing a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟨🟨🟨⬜ 86%](bars/save-saving.svg?v=a8e1847e) | 11 | 6 | 0 | 0 | 7 (4.4 done) | 0 | 0 |  |
| [💾 Kept in the save (one row for both halves)](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟨🟨🟨⬜⬜⬜⬜ 56%](bars/save-both.svg?v=1a641734) | 29 | 16 | 1 | 0 | 52 (26.2 done) | 0 | 20 |  |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-audio.svg?v=81feba96) | 106 | 1 | 1 | 0 | 1 (0.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-video.svg?v=6f52d4b1) | 55 | 0 | 1 | 0 | 2 (1.2 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 89%](bars/features-engine.svg?v=77d34dea) | 121 | 10 | 7 | 0 | 46 (31.8 done) | 0 | 6 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/features-pc-integration.svg?v=f1bd0414) | 50 | 0 | 3 | 0 | 4 (2.6 done) | 0 | 7 |  |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 87%](bars/features-rendering.svg?v=86058925) | 126 | 0 | 7 | 1 | 35 (21.2 done) | 0 | 10 | [15 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
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

Every pull request merged into his master (`3200db4d`, 2026-11-14), checked against our tree.

![🟩🟩🟩🟨🟨🟨🟨🟨⬜⬜ 54%](bars/raffclar-all.svg?v=67708809)

**448** in (195 ported, 253 our own version) · **292** partial · **148** pending · **310** n/a, not taken: CI and build tooling (161); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (40); his deviations from the original, not chosen by us (14); developer tools and behaviour the original does not have (8); his own docs pages (8); force-feedback device behaviour, not ported (user, 2026-10-27) (5); reverts (3); formatting (1).

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

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 1039.

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
