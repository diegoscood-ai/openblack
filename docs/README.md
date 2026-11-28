# openblack wiki

How the original Black & White (v1.42) behaves, with addresses and formulas, and what openblack does.

- **Read it:** open `docs/index.html` through `docs/open_wiki.bat` (Windows), or browse [the pages](SUMMARY.md) here.
- **Start:** the [progress dashboard](wiki/project/dashboard.md) and [Reading this wiki](wiki/project/reading-the-wiki.md).

*This page is written by `docs/tools/progress_stats.py`.*


<a id="where-we-are"></a>

## 📊 Where we are

**🎮 Features of the original**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](wiki/project/bars/features-all.svg?v=e13a017b)

11663 of the original game's 12662 behaviours are done (7423 with a test, 1100 proven in play, 380 of them both, 3520 with neither) and 7 done as the user chose; 749 partial (4% of the rows' worth, 4 not measured yet); 0 inferred, not yet checked against the original. Online is parked: its 211 rows are counted apart, in no score.

**🔀 raffclar's work in our tree**

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 46%](wiki/project/bars/raffclar-all.svg?v=a39482bf)

200 of the 783 pull requests we take from him are in our tree, 314 more in part (303 we have our own version of and 314 not taken are not counted).

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟧 9 of 10](wiki/project/bars/refactor-all.svg?v=b1871e62)

9 of the 10 code goals are reached; in 1 more, what is left stays on purpose.

<a id="features-of-the-original"></a>

## 🎮 Features of the original

Each behaviour of the original game is one row of a [progress file](wiki/project/progress/INDEX.md): done, chosen (done the way the user chose), partial with the share done, inferred (written from an inference, not checked against the original) or pending. The score counts done and chosen as one, a partial row as its share, inferred and pending as nothing. A done row is *with a test* when its notes name one of the tests, and *in play* when they cite a played or real-input run; a row with both counts in each column, and *neither* counts the rest ([the rule](wiki/project/progress/README.md#how-the-dashboard-groups-the-rows)).

🟩 done · 🟢 chosen (done the way the user chose) · 🟨 partial (the share done) · 🟪 inferred (not checked against the original; counts 0) · ⬜ pending

| Family | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 🌍 World | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](wiki/project/bars/family-world.svg?v=ae3b5a1d) | 1030 | 8 | 92 | 2 | 28 (18.1 done) | 0 | 8 |
| 🐉 Creature | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/family-creature.svg?v=68c95fb6) | 1246 | 28 | 42 | 0 | 119 (73.0 done) | 0 | 28 |
| 🏘️ Villagers, towns and animals | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](wiki/project/bars/family-villagers-towns-and-animals.svg?v=f2271247) | 881 | 18 | 32 | 1 | 55 (32.8 done) | 0 | 15 |
| ✋ Player and interface | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/family-player-and-interface.svg?v=f512e399) | 1884 | 227 | 200 | 3 | 130 (90.4 done) | 0 | 22 |
| 📜 Story | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](wiki/project/bars/family-story.svg?v=6536e1b7) | 1795 | 757 | 3104 | 0 | 366 (241.6 done) | 0 | 148 |
| 💾 Save and load | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](wiki/project/bars/family-save-and-load.svg?v=bf265128) | 86 | 45 | 8 | 0 | 23 (12.9 done) | 0 | 7 |
| ⚙️ Engine | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](wiki/project/bars/family-engine.svg?v=20782241) | 501 | 17 | 42 | 1 | 28 (17.2 done) | 0 | 15 |
| ⏸️ Online, parked (in no score) | ![🟩🟩🟩🟩⬜⬜⬜⬜⬜⬜ 36%](wiki/project/bars/family-online-parked.svg?v=97214845) | 30 | 0 | 44 | 0 | 12 (2.7 done) | 1 | 124 |

Save and load gathers the rows about saved games from every domain, so each domain's line below counts the rest of its rows; online play and services are parked (user, 2026-10-24) and counted apart, in no score.

<a id="the-story-land-by-land"></a>

### 📜 The story, land by land

The story domain's rows by the land they belong to: the land's page, the quests the gold and silver scroll indexes list under it, and the sections about that land alone; a row about several lands, or none, is across the lands. The scripts, the rival gods and the easter eggs are not split by land. The story's open issues: [5 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:story).

| Land | Progress | Rows | Gold scrolls | Silver scrolls | The rest |
|---|---|---:|---:|---:|---:|
| [Land 1](wiki/project/progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/story-land-1.svg?v=68de1b01) | 1154 | 97% of 527 | 94% of 555 | 85% of 72 |
| [Land 2](wiki/project/progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](wiki/project/bars/story-land-2.svg?v=0fc7f7bd) | 1258 | 98% of 518 | 98% of 676 | 81% of 64 |
| [Land 3](wiki/project/progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 96%](wiki/project/bars/story-land-3.svg?v=ae9598e6) | 325 | 97% of 178 | 100% of 114 | 80% of 33 |
| [Land 4](wiki/project/progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/story-land-4.svg?v=315d7ca4) | 446 | 95% of 301 | 97% of 99 | 83% of 46 |
| [Land 5](wiki/project/progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/story-land-5.svg?v=f376ae18) | 542 | 96% of 212 | 94% of 299 | 77% of 31 |
| [Tutorial island](wiki/project/progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](wiki/project/bars/story-tutorial-island.svg?v=d0646da0) | 52 | 88% of 51 | — | 60% of 1 |
| [Across the lands](wiki/project/progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/story-across-the-lands.svg?v=80b423e5) | 525 | — | 95% of 110 | 89% of 415 |

*Playable, land by land*: not counted. A land is playable when every system it uses works, and the rows do not say which lands use which system, so that share cannot be read from them; each land's line counts its story rows only.

<details><summary>Every domain</summary>

| Domain | Progress | Done, with a test | Done, in play | Done, neither | Chosen | Partial | Inferred | Pending | Issues |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [Ocean](wiki/project/progress/INDEX.md#ocean) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-ocean.svg?v=f11f245a) | 38 | 0 | 21 | 0 | 0 | 0 | 0 |  |
| [Sky](wiki/project/progress/INDEX.md#sky) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-sky.svg?v=93323051) | 182 | 0 | 0 | 2 | 1 (0.9 done) | 0 | 0 |  |
| [Weather](wiki/project/progress/INDEX.md#weather) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-weather.svg?v=8036232b) | 117 | 0 | 2 | 0 | 1 (0.9 done) | 0 | 0 |  |
| [Physics](wiki/project/progress/INDEX.md#physics) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-physics.svg?v=f55277ec) | 307 | 1 | 27 | 0 | 7 (4.3 done) | 0 | 1 |  |
| [Food, wood and resources](wiki/project/progress/INDEX.md#resources) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-resources.svg?v=4fcfcc49) | 108 | 2 | 0 | 0 | 4 (2.7 done) | 0 | 0 |  |
| [Terrain](wiki/project/progress/INDEX.md#terrain) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-terrain.svg?v=f8b908c9) | 92 | 0 | 19 | 0 | 3 (2.3 done) | 0 | 1 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:terrain) |
| [Nature](wiki/project/progress/INDEX.md#nature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 95%](wiki/project/bars/features-nature.svg?v=ad881fcf) | 186 | 5 | 23 | 0 | 12 (6.9 done) | 0 | 6 |  |
| [Creature](wiki/project/progress/INDEX.md#creature) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/features-creature.svg?v=68c95fb6) | 1246 | 28 | 42 | 0 | 119 (73.0 done) | 0 | 28 | [4 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:creature) |
| [Animals](wiki/project/progress/INDEX.md#animal) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-animal.svg?v=af40f93b) | 83 | 2 | 0 | 0 | 3 (1.9 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:animal) |
| [Buildings](wiki/project/progress/INDEX.md#building) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](wiki/project/bars/features-building.svg?v=56e088ff) | 208 | 4 | 3 | 0 | 6 (3.5 done) | 0 | 5 |  |
| [Towns](wiki/project/progress/INDEX.md#town) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/features-town.svg?v=97e9a8bd) | 169 | 2 | 5 | 1 | 15 (9.2 done) | 0 | 0 |  |
| [Worship and belief](wiki/project/progress/INDEX.md#worship) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](wiki/project/bars/features-worship.svg?v=a159e69f) | 117 | 1 | 1 | 0 | 4 (2.4 done) | 0 | 2 |  |
| [Villagers](wiki/project/progress/INDEX.md#villager) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/features-villager.svg?v=37cee61b) | 304 | 9 | 23 | 0 | 27 (15.9 done) | 0 | 8 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:villager) |
| [Gestures](wiki/project/progress/INDEX.md#gesture) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-gesture.svg?v=02a2c6c0) | 115 | 6 | 0 | 0 | 2 (1.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:gesture) |
| [The hand](wiki/project/progress/INDEX.md#hand) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-hand.svg?v=3410ec9f) | 600 | 121 | 6 | 0 | 19 (12.5 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:hand) |
| [Camera](wiki/project/progress/INDEX.md#camera) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 98%](wiki/project/bars/features-camera.svg?v=4df0c756) | 187 | 28 | 0 | 0 | 11 (7.3 done) | 0 | 0 |  |
| [Interface](wiki/project/progress/INDEX.md#interface) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 98%](wiki/project/bars/features-interface.svg?v=eb2040de) | 277 | 42 | 1 | 3 | 9 (5.4 done) | 0 | 1 |  |
| [Miracles](wiki/project/progress/INDEX.md#miracles) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/features-miracles.svg?v=e0b4dc0c) | 596 | 5 | 127 | 0 | 43 (31.7 done) | 0 | 11 | [3 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:miracles) |
| [Temple](wiki/project/progress/INDEX.md#temple) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 97%](wiki/project/bars/features-temple.svg?v=e9c862f7) | 86 | 5 | 2 | 0 | 8 (5.4 done) | 0 | 0 |  |
| [Skirmish and multiplayer lands](wiki/project/progress/INDEX.md#multiplayer) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 86%](wiki/project/bars/features-multiplayer.svg?v=96f0afbe) | 23 | 20 | 64 | 0 | 38 (26.6 done) | 0 | 10 |  |
| [Land 1](wiki/project/progress/story/land_1.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/story-land-1.svg?v=68de1b01) | 47 | 332 | 693 | 0 | 66 (39.9 done) | 0 | 32 |  |
| [Land 2](wiki/project/progress/story/land_2.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 97%](wiki/project/bars/story-land-2.svg?v=0fc7f7bd) | 36 | 172 | 998 | 0 | 33 (20.1 done) | 0 | 24 |  |
| [Land 3](wiki/project/progress/story/land_3.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 96%](wiki/project/bars/story-land-3.svg?v=ae9598e6) | 5 | 34 | 271 | 0 | 4 (2.4 done) | 0 | 11 |  |
| [Land 4](wiki/project/progress/story/land_4.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 95%](wiki/project/bars/story-land-4.svg?v=315d7ca4) | 9 | 18 | 383 | 0 | 24 (15.0 done) | 0 | 15 |  |
| [Land 5](wiki/project/progress/story/land_5.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 94%](wiki/project/bars/story-land-5.svg?v=f376ae18) | 11 | 12 | 473 | 0 | 22 (13.2 done) | 0 | 26 |  |
| [Tutorial island](wiki/project/progress/story/tutorial_island.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 88%](wiki/project/bars/story-tutorial-island.svg?v=d0646da0) | 7 | 27 | 12 | 0 | 10 (4.6 done) | 0 | 1 |  |
| [Across the lands](wiki/project/progress/INDEX.md#story) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 90%](wiki/project/bars/story-across-the-lands.svg?v=80b423e5) | 238 | 55 | 157 | 0 | 65 (38.1 done) | 0 | 25 |  |
| [Scripts](wiki/project/progress/INDEX.md#scripts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 99%](wiki/project/bars/features-scripts.svg?v=9bfe9565) | 1261 | 43 | 28 | 0 | 53 (35.6 done) | 0 | 0 | [1 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:scripts) |
| [Rival gods](wiki/project/progress/INDEX.md#rival_gods) | ![🟩🟩🟩🟩🟩🟩🟩🟨🟨⬜ 93%](wiki/project/bars/features-rival-gods.svg?v=e6216486) | 155 | 49 | 79 | 0 | 77 (65.8 done) | 0 | 14 |  |
| [Easter eggs](wiki/project/progress/INDEX.md#easter_eggs) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 91%](wiki/project/bars/features-easter-eggs.svg?v=5413d295) | 26 | 15 | 10 | 0 | 12 (6.9 done) | 0 | 0 |  |
| [💾 Loading a save](wiki/project/progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜ 78%](wiki/project/bars/save-loading.svg?v=ed4172b1) | 6 | 6 | 1 | 0 | 0 | 0 | 2 |  |
| [💾 Writing a save](wiki/project/progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 92%](wiki/project/bars/save-saving.svg?v=33fb49af) | 12 | 8 | 0 | 0 | 3 (1.7 done) | 0 | 0 |  |
| [💾 Kept in the save (one row for both halves)](wiki/project/progress/engine/saving_and_loading.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟨⬜ 87%](wiki/project/bars/save-both.svg?v=aca92cb2) | 68 | 31 | 7 | 0 | 20 (11.2 done) | 0 | 5 |  |
| [Sound and music](wiki/project/progress/INDEX.md#audio) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-audio.svg?v=23966646) | 107 | 1 | 1 | 0 | 0 | 0 | 0 |  |
| [Video](wiki/project/progress/INDEX.md#video) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/features-video.svg?v=7784cf15) | 58 | 1 | 2 | 0 | 0 | 0 | 0 |  |
| [Engine and files](wiki/project/progress/INDEX.md#engine) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟨 96%](wiki/project/bars/features-engine.svg?v=520c10ff) | 138 | 14 | 24 | 0 | 11 (7.4 done) | 0 | 4 | [8 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:engine) |
| [Rendering](wiki/project/progress/INDEX.md#rendering) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 93%](wiki/project/bars/features-rendering.svg?v=0a9df6cb) | 146 | 0 | 12 | 1 | 14 (7.8 done) | 0 | 6 | [2 open](https://github.com/diegoscood-ai/openblack/issues?q=is:issue+is:open+label:area:rendering) |
| [PC integration](wiki/project/progress/INDEX.md#pc_integration) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜ 90%](wiki/project/bars/features-pc-integration.svg?v=debd4a48) | 52 | 1 | 3 | 0 | 3 (2.0 done) | 0 | 5 |  |
| [⏸️ Launch switches and stray files](wiki/project/progress/easter_eggs/launch_switches_and_files.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-easter-eggs-launch-switches-and-files.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ File system and packs](wiki/project/progress/engine/file_system_and_packs.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-engine-file-system-and-packs.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Platforms and performance](wiki/project/progress/engine/platforms_and_performance.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-engine-platforms-and-performance.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Hand physics](wiki/project/progress/hand/hand_physics.md) | ![🟨🟨🟨🟨🟨🟨🟨⬜⬜⬜ 70%](wiki/project/bars/online-hand-hand-physics.svg?v=9e666052) | 0 | 0 | 0 | 0 | 1 (0.7 done) | 0 | 0 |  |
| [⏸️ Navigation](wiki/project/progress/hand/navigation.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-hand-navigation.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Other players' hands](wiki/project/progress/hand/other_players_hands.md) | ![🟨🟨🟨⬜⬜⬜⬜⬜⬜⬜ 33%](wiki/project/bars/online-hand-other-players-hands.svg?v=93a605a9) | 0 | 0 | 0 | 0 | 4 (2.0 done) | 0 | 2 |  |
| [⏸️ Holding a miracle and pouring](wiki/project/progress/hand/pouring.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-hand-pouring.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Key bindings](wiki/project/progress/interface/key_bindings.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-interface-key-bindings.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Main menu](wiki/project/progress/interface/main_menu.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-interface-main-menu.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Statistics: what is counted](wiki/project/progress/interface/statistics_counted.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-interface-statistics-counted.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Firestorm](wiki/project/progress/multiplayer/maps/firestorm.md) | ![🟩🟩🟩🟩🟩🟩🟩⬜⬜⬜ 69%](wiki/project/bars/online-multiplayer-maps-firestorm.svg?v=0a06009e) | 0 | 0 | 20 | 0 | 6 | 0 | 3 |  |
| [⏸️ Four Gods](wiki/project/progress/multiplayer/maps/four_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-four-gods.svg?v=e41d6658) | 0 | 0 | 0 | 0 | 0 | 0 | 6 |  |
| [⏸️ Three Gods](wiki/project/progress/multiplayer/maps/three_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-three-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Two Gods](wiki/project/progress/multiplayer/maps/two_gods.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-maps-two-gods.svg?v=045536a1) | 0 | 0 | 0 | 0 | 0 | 0 | 7 |  |
| [⏸️ Multiplayer rules](wiki/project/progress/multiplayer/multiplayer_rules.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-multiplayer-rules.svg?v=8bb63ed6) | 0 | 0 | 0 | 0 | 0 | 0 | 21 |  |
| [⏸️ Network play](wiki/project/progress/multiplayer/network_play.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-network-play.svg?v=99990afc) | 0 | 0 | 0 | 0 | 1 | 0 | 18 |  |
| [⏸️ Online services](wiki/project/progress/multiplayer/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-online-services.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Skirmish](wiki/project/progress/multiplayer/skirmish.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-multiplayer-skirmish.svg?v=7a302ce4) | 0 | 0 | 0 | 0 | 0 | 0 | 2 |  |
| [⏸️ Online services and PC extras](wiki/project/progress/pc_integration/online_services.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-pc-integration-online-services.svg?v=efe8a375) | 0 | 0 | 0 | 0 | 0 | 0 | 9 |  |
| [⏸️ The real clock and calendar](wiki/project/progress/pc_integration/real_clock.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-pc-integration-real-clock.svg?v=f371a1be) | 0 | 0 | 0 | 0 | 0 | 0 | 1 |  |
| [⏸️ Villagers named after the player's contacts](wiki/project/progress/pc_integration/villager_names_from_contacts.md) | ![⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ 0%](wiki/project/bars/online-pc-integration-villager-names-from.svg?v=579d3a06) | 0 | 0 | 0 | 0 | 0 | 1 | 38 |  |
| [⏸️ Playground scripts](wiki/project/progress/scripts/playground_scripts.md) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/online-scripts-playground-scripts.svg?v=e7ae2baf) | 30 | 0 | 24 | 0 | 0 | 0 | 0 |  |

🐞 [Vanilla bugs](wiki/project/progress/vanilla_bugs/README.md): 242 bugs of the original logged (not features, so no score).

</details>

<a id="unification-with-raffclars-tree"></a>
<a id="raffclars-work-in-our-tree"></a>

## 🔀 raffclar's work in our tree

Every pull request merged into his master (`b5fe09fe`, 2026-11-28), checked against our tree.

![🟩🟩🟩🟨🟨🟨🟨⬜⬜⬜ 46%](wiki/project/bars/raffclar-all.svg?v=a39482bf)

**503** in (200 ported, 303 our own version) · **314** partial · **269** pending · **314** n/a, not taken: CI and build tooling (165); online play and services, parked until the end (user, 2026-10-24) (70); his inspector and MCP adapter, parked by decision C-1 (40); his deviations from the original, not chosen by us (14); developer tools and behaviour the original does not have (8); his own docs pages (8); force-feedback device behaviour, not ported (user, 2026-10-27) (5); reverts (3); formatting (1).

The list, pull request by pull request: [raffclar-status.md](wiki/project/raffclar-status.md).

<a id="refactor"></a>

## 🧹 Code clean-up

How far the code follows the [code conventions](wiki/project/refactor-conventions.md#progress-counts); the notes on each goal are there.

🟩 done · 🟧 left on purpose · ⬜ pending

| Goal | Progress |
|---|---|
| [Mutable globals and file- or function-scope statics in game code](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-mutable-globals-and-file-or-function.svg?v=07084c46) |
| [`Game::Instance()` call sites](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-game-instance-call-sites.svg?v=07084c46) |
| [Plain `enum` declarations](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-plain-enum-declarations.svg?v=07084c46) |
| [`new` / `delete` in game code](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-new-delete-in-game-code.svg?v=07084c46) |
| [C arrays](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-c-arrays.svg?v=07084c46) |
| [Direct file reads (`std::ifstream`, `fopen`)](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟧🟧🟧🟧 ~60%](wiki/project/bars/refactor-direct-file-reads-std-ifstream-fopen.svg?v=ffebe33c) |
| [Comments naming the original binary (`fn_…`)](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-comments-naming-the-original-binary-fn.svg?v=07084c46) |
| [Comments quoting the original's addresses](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-comments-quoting-the-originals-addresses.svg?v=07084c46) |
| [Identifiers named after the original binary or unclear](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-identifiers-named-after-the-original.svg?v=07084c46) |
| [Files that fail the CI's clang-format 17 check](wiki/project/refactor-conventions.md#progress-counts) | ![🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩 100%](wiki/project/bars/refactor-files-that-fail-the-cis-clang-format-17.svg?v=07084c46) |

Also counted, with no goal: Locator services 29 → 161; Test files 151 → 1080.

<a id="milestones"></a>

## 🏁 Milestones

- **2026-10-22** · [The checks read the original's meshes](wiki/project/milestones.md#the-checks-read-the-originals-meshes)
- **2026-10-22** · [Mods: the SDK's version, the Mods page and data mods](wiki/project/milestones.md#mods-the-sdks-version-the-mods-page-and-data-mods)
- **2026-10-21** · [Faster builds](wiki/project/milestones.md#faster-builds)
- **2026-10-21** · [The leashes a creature knows in the state hash](wiki/project/milestones.md#the-leashes-a-creature-knows-in-the-state-hash)
- **2026-10-21** · [The top detail level draws the high meshes, chosen from Maximum Detail](wiki/project/milestones.md#the-top-detail-level-and-maximum-detail)
- **2026-10-20** · [Recreate the original, on raffclar's architecture](wiki/project/milestones.md#recreate-the-original-on-raffclars-architecture)

[All milestones](wiki/project/milestones.md).
