# Progress dashboard

How far openblack has come in reproducing Black & White (v1.42), in one page.

<!-- dashboard:static -->

<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](bars/features-all.svg?v=856c0221)

10929 of the original game's 12677 behaviours are done (7132 with a test, 919 proven in play, 327 of them both, 3205 with neither) and 7 done as the user chose; 1180 partial (6% of the rows' worth, 14 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 211 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 49%](bars/raffclar-all.svg?v=2520a16f)

198 of the 714 pull requests we take from him are in our tree, 303 more in part (295 we have our own version of and 313 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟧 9 of 10](bars/refactor-all.svg?v=b1871e62)

9 of the 10 code goals are reached; in 1 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/family-world.svg?v=7677bce7) | 1001 | 3 | 89 | 2 | 52 (34.0 done) | 0 | 22 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](bars/family-creature.svg?v=9505f18d) | 1194 | 22 | 41 | 0 | 165 (101.5 done) | 0 | 41 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](bars/family-villagers-towns-and-animals.svg?v=c681f200) | 830 | 11 | 38 | 1 | 78 (48.4 done) | 0 | 39 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/family-player-and-interface.svg?v=322378a8) | 1830 | 215 | 199 | 3 | 184 (128.6 done) | 0 | 24 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/family-story.svg?v=5a82f1e0) | 1719 | 615 | 2801 | 0 | 612 (388.2 done) | 0 | 404 |
| 💾 Save and load | ![🟩🟩🟩🟩🟩🟩🟨🟨⬜⬜ 76%](bars/family-save-and-load.svg?v=869b2496) | 70 | 39 | 2 | 0 | 43 (24.1 done) | 0 | 13 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/family-engine.svg?v=59911354) | 488 | 14 | 35 | 1 | 46 (27.0 done) | 0 | 18 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟨⬜⬜⬜⬜⬜⬜ 36%](bars/family-online-parked.svg?v=a3d7e11c) | 29 | 0 | 44 | 0 | 13 (3.2 done) | 1 | 124 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/story-land-1.svg?v=103fc037) | 1154 | 95% of 527 | 89% of 555 | 65% of 72 |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-land-2.svg?v=2902b80f) | 1258 | 92% of 518 | 87% of 676 | 66% of 64 |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/story-land-3.svg?v=d631a8a0) | 325 | 86% of 178 | 93% of 114 | 59% of 33 |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/story-land-4.svg?v=40301d43) | 446 | 88% of 301 | 90% of 99 | 64% of 46 |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-5.svg?v=2264b134) | 542 | 82% of 212 | 89% of 299 | 55% of 31 |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-tutorial-island.svg?v=333dc4ca) | 52 | 88% of 51 | — | 0% of 1 |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 85%](bars/story-across-the-lands.svg?v=df928c48) | 525 | — | 88% of 110 | 84% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-ocean.svg?v=f11f245a) | 37 | 0 | 22 | 0 | 0 | 0 | 0 |  |
| [Sky](progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-weather.svg?v=8036232b) | 117 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Physics](progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-physics.svg?v=3ce3dfa2) | 299 | 0 | 27 | 0 | 14 (9.4 done) | 0 | 2 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:physics) |
| [Terrain](progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-terrain.svg?v=9f71461d) | 92 | 0 | 19 | 0 | 3 (2.1 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Food, wood and resources](progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 92%](bars/features-resources.svg?v=95cf0b95) | 99 | 1 | 0 | 0 | 9 (6.1 done) | 0 | 6 |  |
| [Nature](progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-nature.svg?v=aeef4fb8) | 175 | 2 | 19 | 0 | 24 (14.5 done) | 0 | 13 |  |
| [Creature](progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](bars/features-creature.svg?v=9505f18d) | 1194 | 22 | 41 | 0 | 165 (101.5 done) | 0 | 41 | [13 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Animals](progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-animal.svg?v=91a24d3e) | 81 | 2 | 0 | 0 | 5 (3.1 done) | 0 | 0 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Worship and belief](progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-worship.svg?v=0e2c4243) | 114 | 0 | 1 | 0 | 6 (3.5 done) | 0 | 3 |  |
| [Towns](progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](bars/features-town.svg?v=32e5ee2e) | 160 | 1 | 3 | 1 | 24 (16.1 done) | 0 | 2 |  |
| [Buildings](progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 91%](bars/features-building.svg?v=b2ecc2fe) | 194 | 2 | 2 | 0 | 16 (8.8 done) | 0 | 12 |  |
| [Villagers](progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 91%](bars/features-villager.svg?v=4af77484) | 281 | 6 | 32 | 0 | 27 (16.9 done) | 0 | 22 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Camera](progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-camera.svg?v=99f75215) | 185 | 28 | 0 | 0 | 13 (9.0 done) | 0 | 0 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:camera) |
| [Gestures](progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-gesture.svg?v=5683c37e) | 112 | 5 | 0 | 0 | 4 (2.7 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [The hand](progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](bars/features-hand.svg?v=e4d6d65f) | 594 | 116 | 5 | 0 | 28 (18.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Interface](progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-interface.svg?v=e51de121) | 271 | 40 | 1 | 3 | 13 (8.0 done) | 0 | 2 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:interface) |
| [Temple](progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](bars/features-temple.svg?v=e9c862f7) | 86 | 5 | 2 | 0 | 8 (5.4 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:temple) |
| [Miracles](progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](bars/features-miracles.svg?v=4619dca3) | 560 | 2 | 127 | 0 | 79 (57.5 done) | 0 | 11 | [6 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Skirmish and multiplayer lands](progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](bars/features-multiplayer.svg?v=6fb9f92c) | 22 | 19 | 64 | 0 | 39 (27.5 done) | 0 | 10 |  |
| [Land 1](progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/story-land-1.svg?v=103fc037) | 35 | 298 | 652 | 0 | 118 (65.7 done) | 0 | 59 |  |
| [Land 2](progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](bars/story-land-2.svg?v=2902b80f) | 25 | 129 | 890 | 0 | 109 (65.8 done) | 0 | 107 |  |
| [Land 3](progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/story-land-3.svg?v=d631a8a0) | 3 | 19 | 252 | 0 | 7 (4.9 done) | 0 | 44 |  |
| [Land 4](progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 86%](bars/story-land-4.svg?v=40301d43) | 7 | 12 | 340 | 0 | 44 (26.9 done) | 0 | 45 |  |
| [Land 5](progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 84%](bars/story-land-5.svg?v=2264b134) | 8 | 8 | 416 | 0 | 44 (26.4 done) | 0 | 68 |  |
| [Tutorial island](progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](bars/story-tutorial-island.svg?v=333dc4ca) | 7 | 27 | 12 | 0 | 9 (4.0 done) | 0 | 2 |  |
| [Across the lands](progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 85%](bars/story-across-the-lands.svg?v=df928c48) | 229 | 40 | 141 | 0 | 76 (46.5 done) | 0 | 50 |  |
| [Scripts](progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](bars/features-scripts.svg?v=063cdc66) | 1242 | 32 | 27 | 0 | 79 (51.5 done) | 0 | 3 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Easter eggs](progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](bars/features-easter-eggs.svg?v=772487a2) | 26 | 15 | 9 | 0 | 13 (7.4 done) | 0 | 0 |  |
| [Rival gods](progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟩🟨🟨🟨⬜ 86%](bars/features-rival-gods.svg?v=31374a58) | 137 | 35 | 62 | 0 | 113 (89.0 done) | 0 | 26 |  |
| [💾 Loading a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩⬜⬜⬜ 72%](bars/save-loading.svg?v=60fb25c5) | 5 | 6 | 0 | 0 | 1 (0.5 done) | 0 | 2 |  |
| [💾 Writing a save](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 88%](bars/save-saving.svg?v=b89d7732) | 12 | 7 | 0 | 0 | 6 (3.8 done) | 0 | 0 |  |
| [💾 Kept in the save (one row for both halves)](progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟨🟨⬜⬜⬜ 74%](bars/save-both.svg?v=aad11525) | 53 | 26 | 2 | 0 | 36 (19.8 done) | 0 | 11 |  |
| [Sound and music](progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](bars/features-audio.svg?v=81feba96) | 106 | 1 | 1 | 0 | 1 (0.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:audio) |
| [Video](progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](bars/features-video.svg?v=5bf7063c) | 56 | 0 | 1 | 0 | 2 (1.3 done) | 0 | 1 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:video) |
| [Engine and files](progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](bars/features-engine.svg?v=4c8e4bca) | 135 | 12 | 22 | 0 | 17 (11.0 done) | 0 | 4 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [PC integration](progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 90%](bars/features-pc-integration.svg?v=debd4a48) | 51 | 1 | 4 | 0 | 3 (2.0 done) | 0 | 5 |  |
| [Rendering](progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](bars/features-rendering.svg?v=77b4cbc7) | 140 | 0 | 7 | 1 | 23 (12.2 done) | 0 | 8 | [7 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
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
| [⏸️ Playground scripts](progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](bars/online-scripts-playground-scripts.svg?v=2d2cd500) | 29 | 0 | 24 | 0 | 1 (0.5 done) | 0 | 0 |  |

🐞 [Vanilla bugs](progress/vanilla_bugs/README.md): 242 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`83da7006`, 2026-11-21), checked against our tree.

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 49%](bars/raffclar-all.svg?v=2520a16f)

**493** in (198 ported, 295 our own version) · **303** partial · **213** pending · **313** n/a, not taken: CI and build tooling (164); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (40); his deviations from the original, not chosen by us (14); developer tools and behaviour the original does not have (8); his own docs pages (8); force-feedback device behaviour, not ported (user, 2026-10-27) (5); reverts (3); formatting (1).

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

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 1073.

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
