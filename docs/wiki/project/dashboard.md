# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-all.svg?v=e13a017b)

11663 of the original game's 12662 behaviours are done (7423 with a test, 1100 proven in play, 380 of them both, 3520 with neither) and 7 done as the user chose; 749 partial (4% of the rows' worth, 4 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 211 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 46%](bars/raffclar-all.svg?v=a39482bf)

200 of the 783 pull requests we take from him are in our tree, 314 more in part (303 we have our own version of and 314 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟧 9 of 10](bars/refactor-all.svg?v=b1871e62)

9 of the 10 code goals are reached; in 1 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/family-world.svg?v=ae3b5a1d) | 1030 | 8 | 92 | 2 | 28 (18.1 done) | 0 | 8 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/family-creature.svg?v=68c95fb6) | 1246 | 28 | 42 | 0 | 119 (73.0 done) | 0 | 28 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/family-villagers-towns-and-animals.svg?v=f2271247) | 881 | 18 | 32 | 1 | 55 (32.8 done) | 0 | 15 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/family-player-and-interface.svg?v=f512e399) | 1884 | 227 | 200 | 3 | 130 (90.4 done) | 0 | 22 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/family-story.svg?v=6536e1b7) | 1795 | 757 | 3104 | 0 | 366 (241.6 done) | 0 | 148 |
| 💾 Save and load | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/family-save-and-load.svg?v=bf265128) | 86 | 45 | 8 | 0 | 23 (12.9 done) | 0 | 7 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/family-engine.svg?v=20782241) | 501 | 17 | 42 | 1 | 28 (17.2 done) | 0 | 15 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 36%](bars/family-online-parked.svg?v=97214845) | 30 | 0 | 44 | 0 | 12 (2.7 done) | 1 | 124 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/story-land-1.svg?v=68de1b01) | 1154 | 97% of 527 | 94% of 555 | 85% of 72 |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](bars/story-land-2.svg?v=0fc7f7bd) | 1258 | 98% of 518 | 98% of 676 | 81% of 64 |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 96%](bars/story-land-3.svg?v=ae9598e6) | 325 | 97% of 178 | 100% of 114 | 80% of 33 |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/story-land-4.svg?v=315d7ca4) | 446 | 95% of 301 | 97% of 99 | 83% of 46 |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/story-land-5.svg?v=f376ae18) | 542 | 96% of 212 | 94% of 299 | 77% of 31 |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-tutorial-island.svg?v=d0646da0) | 52 | 88% of 51 | — | 60% of 1 |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/story-across-the-lands.svg?v=80b423e5) | 525 | — | 95% of 110 | 89% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 38 | 0 | 21 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-weather.svg?v=8036232b) | 117 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-physics.svg?v=f55277ec) | 307 | 1 | 27 | 0 | 7 (4.3 done) | 0 | 1 |  |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-resources.svg?v=4fcfcc49) | 108 | 2 | 0 | 0 | 4 (2.7 done) | 0 | 0 |  |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-terrain.svg?v=f8b908c9) | 92 | 0 | 19 | 0 | 3 (2.3 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/features-nature.svg?v=ad881fcf) | 186 | 5 | 23 | 0 | 12 (6.9 done) | 0 | 6 |  |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/features-creature.svg?v=68c95fb6) | 1246 | 28 | 42 | 0 | 119 (73.0 done) | 0 | 28 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-animal.svg?v=af40f93b) | 83 | 2 | 0 | 0 | 3 (1.9 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](bars/features-building.svg?v=56e088ff) | 208 | 4 | 3 | 0 | 6 (3.5 done) | 0 | 5 |  |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-town.svg?v=97e9a8bd) | 169 | 2 | 5 | 1 | 15 (9.2 done) | 0 | 0 |  |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](bars/features-worship.svg?v=a159e69f) | 117 | 1 | 1 | 0 | 4 (2.4 done) | 0 | 2 |  |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/features-villager.svg?v=37cee61b) | 304 | 9 | 23 | 0 | 27 (15.9 done) | 0 | 8 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-gesture.svg?v=02a2c6c0) | 115 | 6 | 0 | 0 | 2 (1.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-hand.svg?v=3410ec9f) | 600 | 121 | 6 | 0 | 19 (12.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-camera.svg?v=4df0c756) | 187 | 28 | 0 | 0 | 11 (7.3 done) | 0 | 0 |  |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-interface.svg?v=eb2040de) | 277 | 42 | 1 | 3 | 9 (5.4 done) | 0 | 1 |  |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-miracles.svg?v=e0b4dc0c) | 596 | 5 | 127 | 0 | 43 (31.7 done) | 0 | 11 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-temple.svg?v=e9c862f7) | 86 | 5 | 2 | 0 | 8 (5.4 done) | 0 | 0 |  |
| [Skirmish and multiplayer lands](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](bars/features-multiplayer.svg?v=96f0afbe) | 23 | 20 | 64 | 0 | 38 (26.6 done) | 0 | 10 |  |
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/story-land-1.svg?v=68de1b01) | 47 | 332 | 693 | 0 | 66 (39.9 done) | 0 | 32 |  |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](bars/story-land-2.svg?v=0fc7f7bd) | 36 | 172 | 998 | 0 | 33 (20.1 done) | 0 | 24 |  |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 96%](bars/story-land-3.svg?v=ae9598e6) | 5 | 34 | 271 | 0 | 4 (2.4 done) | 0 | 11 |  |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/story-land-4.svg?v=315d7ca4) | 9 | 18 | 383 | 0 | 24 (15.0 done) | 0 | 15 |  |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/story-land-5.svg?v=f376ae18) | 11 | 12 | 473 | 0 | 22 (13.2 done) | 0 | 26 |  |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-tutorial-island.svg?v=d0646da0) | 7 | 27 | 12 | 0 | 10 (4.6 done) | 0 | 1 |  |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/story-across-the-lands.svg?v=80b423e5) | 238 | 55 | 157 | 0 | 65 (38.1 done) | 0 | 25 |  |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-scripts.svg?v=9bfe9565) | 1261 | 43 | 28 | 0 | 53 (35.6 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 93%](bars/features-rival-gods.svg?v=e6216486) | 155 | 49 | 79 | 0 | 77 (65.8 done) | 0 | 14 |  |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/features-easter-eggs.svg?v=5413d295) | 26 | 15 | 10 | 0 | 12 (6.9 done) | 0 | 0 |  |
| [💾 Loading a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 78%](bars/save-loading.svg?v=ed4172b1) | 6 | 6 | 1 | 0 | 0 | 0 | 2 |  |
| [💾 Writing a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 92%](bars/save-saving.svg?v=33fb49af) | 12 | 8 | 0 | 0 | 3 (1.7 done) | 0 | 0 |  |
| [💾 Kept in the save (one row for both halves)](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/save-both.svg?v=aca92cb2) | 68 | 31 | 7 | 0 | 20 (11.2 done) | 0 | 5 |  |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-audio.svg?v=23966646) | 107 | 1 | 1 | 0 | 0 | 0 | 0 |  |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-video.svg?v=7784cf15) | 58 | 1 | 2 | 0 | 0 | 0 | 0 |  |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-engine.svg?v=520c10ff) | 138 | 14 | 24 | 0 | 11 (7.4 done) | 0 | 4 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](bars/features-rendering.svg?v=0a9df6cb) | 146 | 0 | 12 | 1 | 14 (7.8 done) | 0 | 6 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 90%](bars/features-pc-integration.svg?v=debd4a48) | 52 | 1 | 3 | 0 | 3 (2.0 done) | 0 | 5 |  |
| [⏸️ Launch switches and stray files](progress/easter_eggs/launch_switches_and_files.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-easter-eggs-launch-switches-and-files.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ File system and packs](progress/engine/file_system_and_packs.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-engine-file-system-and-packs.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Platforms and performance](progress/engine/platforms_and_performance.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-engine-platforms-and-performance.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Hand physics](progress/hand/hand_physics.md) | ![🟨🟨🟨🟨🟨🟨🟨⬜⬜⬜ 70%](bars/online-hand-hand-physics.svg?v=9e666052) | 0 | 0 | 0 | 0 | 1 (0.7 done) | 0 | 0 |  |
| [⏸️ Navigation](progress/hand/navigation.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-hand-navigation.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Other players' hands](progress/hand/other_players_hands.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 33%](bars/online-hand-other-players-hands.svg?v=93a605a9) | 0 | 0 | 0 | 0 | 4 (2.0 done) | 0 | 2 |  |
| [⏸️ Holding a miracle and pouring](progress/hand/pouring.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-hand-pouring.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Key bindings](progress/interface/key_bindings.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-interface-key-bindings.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Main menu](progress/interface/main_menu.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-interface-main-menu.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Statistics: what is counted](progress/interface/statistics_counted.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-interface-statistics-counted.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Firestorm](progress/multiplayer/maps/firestorm.md) | ![🟩🟩🟩🟩🟩🟩🟩⬜⬜⬜ 69%](bars/online-multiplayer-maps-firestorm.svg?v=0a06009e) | 0 | 0 | 20 | 0 | 6 | 0 | 3 |  |
| [⏸️ Four Gods](progress/multiplayer/maps/four_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-four-gods.svg?v=e41d6658) | 0 | 0 | 0 | 0 | 0 | 0 | 6 |  |
| [⏸️ Three Gods](progress/multiplayer/maps/three_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-three-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Two Gods](progress/multiplayer/maps/two_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-maps-two-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Multiplayer rules](progress/multiplayer/multiplayer_rules.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-multiplayer-rules.svg?v=8bb63ed6) | 0 | 0 | 0 | 0 | 0 | 0 | 21 |  |
| [⏸️ Network play](progress/multiplayer/network_play.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-network-play.svg?v=99990afc) | 0 | 0 | 0 | 0 | 1 | 0 | 18 |  |
| [⏸️ Online services](progress/multiplayer/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-online-services.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Skirmish](progress/multiplayer/skirmish.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-skirmish.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Online services and PC extras](progress/pc_integration/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-online-services.svg?v=efe8a375) | 0 | 0 | 0 | 0 | 0 | 0 | 9 |  |
| [⏸️ The real clock and calendar](progress/pc_integration/real_clock.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-real-clock.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Villagers named after the player's contacts](progress/pc_integration/villager_names_from_contacts.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-villager-names-from.svg?v=579d3a06) | 0 | 0 | 0 | 0 | 0 | 1 | 38 |  |
| [⏸️ Playground scripts](progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/online-scripts-playground-scripts.svg?v=e7ae2baf) | 30 | 0 | 24 | 0 | 0 | 0 | 0 |  |

🐞 [Vanilla bugs](progress/vanilla_bugs/README.md): 242 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`b5fe09fe`, 2026-11-28), checked against our tree.

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 46%](bars/raffclar-all.svg?v=a39482bf)

**503** in (200 ported, 303 our own version) · **314** partial · **269** pending · **314** n/a, not taken: CI and build tooling (165); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (40); his deviations from the original, not chosen by us (14); developer tools and behaviour the original does not have (8); his own docs pages (8); force-feedback device behaviour, not ported (user, 2026-10-27) (5); reverts (3); formatting (1).

The list, pull request by pull request: [raffclar-status.md](raffclar-status.md).

<a id="refactor"></a>

## 🧹 Code clean-up

How far the code follows the [code conventions](refactor-conventions.md#progress-counts); the notes on each goal are there.

🟩 done · 🟧 left on purpose · ⬜ pending

| Goal | Progress |
|---|---|
| [Mutable globals and file- or function-scope statics in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-mutable-globals-and-file-or-function.svg?v=07084c46) |
| [`Game::Instance()` call sites](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-game-instance-call-sites.svg?v=07084c46) |
| [Plain `enum` declarations](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-plain-enum-declarations.svg?v=07084c46) |
| [`new` / `delete` in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-new-delete-in-game-code.svg?v=07084c46) |
| [C arrays](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-c-arrays.svg?v=07084c46) |
| [Direct file reads (`std::ifstream`, `fopen`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~60%](bars/refactor-direct-file-reads-std-ifstream-fopen.svg?v=ffebe33c) |
| [Comments naming the original binary (`fn_…`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-naming-the-original-binary-fn.svg?v=07084c46) |
| [Comments quoting the original's addresses](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-quoting-the-originals-addresses.svg?v=07084c46) |
| [Identifiers named after the original binary or unclear](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-identifiers-named-after-the-original.svg?v=07084c46) |
| [Files that fail the CI's clang-format 17 check](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 1080.

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
