# Land 4

Khazar's land again, now ruined by Nemesis: meteors fall, a village has been turned undead and ogres guard the way. The
player rebuilds, helps the survivors and the creature is given the first of the creeds that will protect it.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/15 done, 0 partial — 0%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land4_script.md |
| The player comes through the vortex and finds the land ruined; the creature is kept away from the vortex | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); keeping the creature away (`CreatureDoAction`) is a stub |
| A man explains what Nemesis has done | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md) (its natives are mostly real; the log is a stub) |
| Meteors fall on the land: fire, lightning and darkness meteors, aimed near the player's towns | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); the fireballs, storms and clock natives are real; see ../miracles/ |
| The player's home town is built up and its people are kept from exploring until the story allows | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md); moving the creature by script is not done |
| The creature is given the creed | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md#the-creed): `InCreatureHand` and `SetCreatureCreedProperties` are stubs |

## Gold scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "The Heartbroken Man" — a nomad's story; how the player answers sets the alignment | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/the_heartbroken_man.md](gold_scrolls/the_heartbroken_man.md): most natives are real; the health and held reads are not handled |
| Gold scroll: "Undead Village" — a village of skeletons; set right by raising its two sunken totems | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md): skeletons and script-made totems are not done |
| Gold scroll: "The Totem Puzzle" — a bell memory game at five bell towers by the Japanese village: the towers ring rounds of 3, 5, 7 and 9 notes and the player clicks them back in the same order; the totems to raise belong to Undead Village | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/the_totem_puzzle.md](gold_scrolls/the_totem_puzzle.md): the bell towers (`CREATE` of a totem) are never made |
| Gold scroll: "The Defending Ogres" — the ogre Sleg and his family guard the way with a guardian stone; gremlins attack the creature | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/the_defending_ogres.md](gold_scrolls/the_defending_ogres.md): Sleg and the gremlins are never made (`CreatureCreateRelativeToCreature` is a stub) |
| Gold scroll: leave through the vortex — the way to Nemesis's land | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/leave_through_the_vortex_land_4.md](gold_scrolls/leave_through_the_vortex_land_4.md) (its script has no stubs); vortex mechanics: [portals.md](portals.md) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Treacherous Path" — a blind woman walks to her brother with healing potions; the player keeps her safe | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [silver_scrolls/the_treacherous_path.md](silver_scrolls/the_treacherous_path.md) |
| Silver scroll: "The Fish Puzzle" — a boy wants to be a fisherman; tapping the water herds the fish away from the hand | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [silver_scrolls/the_fish_puzzle.md](silver_scrolls/the_fish_puzzle.md) |
| Silver scroll (no title in the game's text): the creature breeder returns with creatures to swap | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see challenges_and_rewards.md; [silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |
| Bronze puzzles: the Theseus maze, whose prize falls from the sky, and the Japanese village's totem puzzle (raise all the totems), which gives a shield miracle dispenser | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [minigames.md](minigames.md) and [gold_scrolls/the_totem_puzzle.md](gold_scrolls/the_totem_puzzle.md#quirks-unused-and-cut-parts); `CREATE` of a puzzle game is real (`src/ECS/PuzzleGames`) |
