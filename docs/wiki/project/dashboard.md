# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟨⬜⬜⬜ 65%](bars/features-all.svg?v=ad0d5964)

7164 of the original game's 13015 behaviours are done (5704 with a test, 146 proven in play, 71 of them both, 1385 with neither) and 5 done as the user chose; 2396 partial (10% of the rows' worth, 174 not measured yet); 221 inferred, not yet checked against the original. Online is parked: its 137 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟩🟨🟨🟨⬜⬜⬜ 55%](bars/raffclar-all.svg?v=2b7f8e1e)

332 of his 788 pull requests are in our tree, 203 more in part.

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 6 of 10](bars/refactor-all.svg?v=d6a48625)

6 of the 10 code goals are reached; in 4 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 92%](bars/family-world.svg?v=66b46667) | 894 | 0 | 85 | 2 | 123 (92.6 done) | 18 | 48 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟨🟨🟪⬜⬜ 68%](bars/family-creature.svg?v=3820e975) | 805 | 0 | 7 | 0 | 399 (238.6 done) | 134 | 192 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/family-villagers-towns-and-animals.svg?v=9e0a2b77) | 706 | 2 | 31 | 0 | 137 (88.9 done) | 15 | 112 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](bars/family-player-and-interface.svg?v=580460b6) | 1450 | 69 | 188 | 2 | 437 (261.2 done) | 47 | 196 |
| 📜 Story | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 50%](bars/family-story.svg?v=ba1d0481) | 1431 | 75 | 1056 | 0 | 1152 (574.8 done) | 5 | 2499 |
| 💾 Save and load | ![🟨🟨⬜⬜⬜⬜⬜⬜⬜⬜ 18%](bars/family-save-and-load.svg?v=9285af12) | 5 | 0 | 1 | 0 | 42 (18.0 done) | 0 | 82 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 78%](bars/family-engine.svg?v=cac6c85f) | 413 | 0 | 17 | 1 | 106 (66.8 done) | 2 | 100 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩🟩⬜⬜⬜⬜⬜ 54%](bars/family-online-parked.svg?v=f180e84e) | 29 | 0 | 44 | 0 | 9 (0.8 done) | 0 | 55 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 44%](bars/story-land-1.svg?v=874bbe83) | 1158 | 40% of 529 | 49% of 557 | 27% of 72 |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 42%](bars/story-land-2.svg?v=c2932cd8) | 1261 | 81% of 518 | 16% of 679 | 2% of 64 |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩⬜⬜⬜⬜⬜⬜⬜ 34%](bars/story-land-3.svg?v=66a83103) | 326 | 59% of 178 | 3% of 115 | 0% of 33 |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 45%](bars/story-land-4.svg?v=e472f706) | 446 | 56% of 301 | 31% of 99 | 1% of 46 |
| [Land 5](progress/story/land_5.md) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 17%](bars/story-land-5.svg?v=d004b1f7) | 543 | 36% of 212 | 5% of 300 | 0% of 31 |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 26%](bars/story-tutorial-island.svg?v=0aa40008) | 52 | 26% of 51 | — | 0% of 1 |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 53%](bars/story-across-the-lands.svg?v=954d243c) | 526 | — | 10% of 111 | 65% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 37 | 0 | 22 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-sky.svg?v=5d1775bd) | 181 | 0 | 0 | 2 | 2 (1.9 done) | 0 | 0 |  |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-weather.svg?v=f07a209e) | 116 | 0 | 2 | 0 | 1 (0.8 done) | 0 | 0 |  |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-terrain.svg?v=892c45d9) | 89 | 0 | 19 | 0 | 6 (4.8 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 93%](bars/features-physics.svg?v=fe57ef62) | 256 | 0 | 26 | 0 | 51 (37.7 done) | 4 | 5 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics,area:particles) |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 87%](bars/features-resources.svg?v=84dcbf64) | 82 | 0 | 0 | 0 | 22 (18.0 done) | 1 | 10 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:resources) |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 76%](bars/features-nature.svg?v=1720a2d1) | 133 | 0 | 16 | 0 | 41 (29.5 done) | 13 | 32 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:nature) |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟨🟨🟪⬜⬜ 68%](bars/features-creature.svg?v=3820e975) | 805 | 0 | 7 | 0 | 399 (238.6 done) | 134 | 192 | [24 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 89%](bars/features-worship.svg?v=6634a4de) | 108 | 0 | 0 | 0 | 8 (4.7 done) | 2 | 8 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:worship) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](bars/features-animal.svg?v=e6194c56) | 62 | 0 | 0 | 0 | 18 (13.7 done) | 3 | 5 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 83%](bars/features-building.svg?v=62beaaa2) | 162 | 1 | 2 | 0 | 41 (26.3 done) | 3 | 20 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:building) |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 82%](bars/features-villager.svg?v=d4ea1339) | 254 | 1 | 29 | 0 | 30 (16.2 done) | 5 | 47 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 76%](bars/features-town.svg?v=61e0163e) | 120 | 0 | 0 | 0 | 40 (27.9 done) | 2 | 32 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:town) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](bars/features-gesture.svg?v=b5592f56) | 106 | 0 | 0 | 0 | 3 (2.6 done) | 0 | 5 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-temple.svg?v=d3334e9f) | 70 | 0 | 1 | 0 | 18 (12.8 done) | 0 | 4 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/features-hand.svg?v=4d87bf4c) | 510 | 65 | 1 | 0 | 87 (54.1 done) | 18 | 37 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 85%](bars/features-miracles.svg?v=75d02123) | 410 | 0 | 125 | 0 | 192 (135.8 done) | 12 | 46 | [9 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 82%](bars/features-camera.svg?v=fdb1c5cf) | 133 | 0 | 0 | 0 | 55 (31.2 done) | 1 | 11 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟪⬜ 81%](bars/features-interface.svg?v=e9182442) | 217 | 4 | 0 | 2 | 44 (24.8 done) | 16 | 22 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Skirmish and multiplayer lands](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 37%](bars/features-multiplayer.svg?v=c5942a28) | 4 | 0 | 61 | 0 | 38 | 0 | 71 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:multiplayer) |
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 44%](bars/story-land-1.svg?v=874bbe83) | 13 | 18 | 341 | 0 | 336 (134.1 done) | 0 | 450 |  |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 42%](bars/story-land-2.svg?v=c2932cd8) | 5 | 53 | 391 | 0 | 146 (84.8 done) | 0 | 666 |  |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩⬜⬜⬜⬜⬜⬜⬜ 34%](bars/story-land-3.svg?v=66a83103) | 1 | 0 | 83 | 0 | 46 (25.5 done) | 0 | 196 |  |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 45%](bars/story-land-4.svg?v=e472f706) | 0 | 0 | 163 | 0 | 65 (38.0 done) | 0 | 218 |  |
| [Land 5](progress/story/land_5.md) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 17%](bars/story-land-5.svg?v=d004b1f7) | 0 | 0 | 54 | 0 | 71 (37.4 done) | 0 | 418 |  |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 26%](bars/story-tutorial-island.svg?v=0aa40008) | 0 | 0 | 1 | 0 | 38 (12.4 done) | 0 | 13 |  |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 53%](bars/story-across-the-lands.svg?v=954d243c) | 185 | 3 | 9 | 0 | 134 (82.3 done) | 0 | 195 |  |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/features-scripts.svg?v=fc3f3897) | 1131 | 1 | 7 | 0 | 192 (109.7 done) | 5 | 120 | [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 37%](bars/features-rival-gods.svg?v=9216ec29) | 89 | 0 | 7 | 0 | 110 (45.1 done) | 0 | 172 |  |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟨⬜⬜⬜⬜⬜⬜⬜⬜ 17%](bars/features-easter-eggs.svg?v=bf741f40) | 7 | 0 | 0 | 0 | 14 (5.5 done) | 0 | 51 |  |
| [💾 Loading a save](progress/engine/saving_and_loading.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 28%](bars/save-loading.svg?v=a7a5685b) | 0 | 0 | 0 | 0 | 5 (2.5 done) | 0 | 4 |  |
| [💾 Writing a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟨🟨⬜⬜⬜⬜⬜⬜ 39%](bars/save-saving.svg?v=c1026781) | 4 | 0 | 0 | 0 | 8 (3.1 done) | 0 | 6 |  |
| [💾 Kept in the save (one row for both halves)](progress/engine/saving_and_loading.md) | ![🟨⬜⬜⬜⬜⬜⬜⬜⬜⬜ 14%](bars/save-both.svg?v=b8213e15) | 1 | 0 | 1 | 0 | 29 (12.4 done) | 0 | 72 |  |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/features-audio.svg?v=a9b435bb) | 105 | 0 | 1 | 0 | 1 (0.5 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-video.svg?v=6f52d4b1) | 55 | 0 | 1 | 0 | 2 (1.2 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟨🟨🟨⬜⬜ 79%](bars/features-engine.svg?v=f5d9fb49) | 96 | 0 | 5 | 0 | 68 (45.8 done) | 0 | 18 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟨⬜⬜ 77%](bars/features-rendering.svg?v=ef9be164) | 114 | 0 | 7 | 1 | 33 (18.1 done) | 0 | 26 | [14 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟨⬜⬜⬜⬜⬜ 45%](bars/features-pc-integration.svg?v=033002eb) | 43 | 0 | 3 | 0 | 2 (1.2 done) | 2 | 54 |  |
| [⏸️ Firestorm](progress/multiplayer/maps/firestorm.md) | ![🟩🟩🟩🟩🟩🟩⬜⬜⬜⬜ 65%](bars/online-multiplayer-maps-firestorm.svg?v=026c3517) | 0 | 0 | 20 | 0 | 6 | 0 | 5 |  |
| [⏸️ Multiplayer rules](progress/multiplayer/multiplayer_rules.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-multiplayer-rules.svg?v=8bb63ed6) | 0 | 0 | 0 | 0 | 0 | 0 | 21 |  |
| [⏸️ Network play](progress/multiplayer/network_play.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-network-play.svg?v=99990afc) | 0 | 0 | 0 | 0 | 1 | 0 | 18 |  |
| [⏸️ Online services](progress/multiplayer/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-multiplayer-online-services.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Online services and PC extras](progress/pc_integration/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](bars/online-pc-integration-online-services.svg?v=efe8a375) | 0 | 0 | 0 | 0 | 0 | 0 | 9 |  |
| [⏸️ Playground scripts](progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/online-scripts-playground-scripts.svg?v=938a427c) | 29 | 0 | 24 | 0 | 2 (0.8 done) | 0 | 0 |  |

🐞 [Vanilla bugs](progress/vanilla_bugs/README.md): 241 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`8b8f9655`, 2026-10-31), checked against our tree.

![🟩🟩🟩🟩🟨🟨🟨⬜⬜⬜ 55%](bars/raffclar-all.svg?v=2b7f8e1e)

**332** in · **203** partial · **253** pending · **183** n/a, not taken: online play and services, parked until the end (user, 2026-10-24) (62); CI and build tooling (60); his inspector and MCP adapter, parked by decision C-1 (34); his deviations from the original, not chosen by us (8); his own docs pages (8); developer tools and behaviour the original does not have (7); reverts (2); formatting (1); force-feedback device behaviour, not ported (user, 2026-10-27) (1).

The list, pull request by pull request: [raffclar-status.md](raffclar-status.md).

<a id="refactor"></a>

## 🧹 Code clean-up

How far the code follows the [code conventions](refactor-conventions.md#progress-counts); the notes on each goal are there.

🟩 done · 🟧 left on purpose · ⬜ pending

| Goal | Progress |
|---|---|
| [Mutable globals and file- or function-scope statics in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~96%](bars/refactor-mutable-globals-and-file-or-function.svg?v=6139d414) |
| [`Game::Instance()` call sites](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-game-instance-call-sites.svg?v=07084c46) |
| [Plain `enum` declarations](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~64%](bars/refactor-plain-enum-declarations.svg?v=7d4b6c3e) |
| [`new` / `delete` in game code](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-new-delete-in-game-code.svg?v=07084c46) |
| [C arrays](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-c-arrays.svg?v=07084c46) |
| [Direct file reads (`std::ifstream`, `fopen`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~60%](bars/refactor-direct-file-reads-std-ifstream-fopen.svg?v=ffebe33c) |
| [Comments naming the original binary (`fn_…`)](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-naming-the-original-binary-fn.svg?v=07084c46) |
| [Comments quoting the original's addresses](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-comments-quoting-the-originals-addresses.svg?v=07084c46) |
| [Identifiers named after the original binary or unclear](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 ~95%](bars/refactor-identifiers-named-after-the-original.svg?v=e87ec08c) |
| [Files that fail the CI's clang-format 17 check](refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 941.

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
