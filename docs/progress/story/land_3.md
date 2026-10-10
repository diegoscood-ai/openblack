# Land 3

Lethys's prison land: the player arrives without the creature, which Lethys holds bound, and with only a few believers.
Lethys sends possessed wolves and fanatics; a monk helps; freeing the creature opens the way on.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/11 done, 0 partial — 0%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land3_script.md |
| The player comes through the vortex alone and Lethys taunts them | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md) |
| The land's own weather is switched off for the story | todo | `PauseUnpauseClimateSystem` and `KillStormsInArea` are real (`src/Magic/Script/CHLWeather.cpp`); never reached: `LOAD_MAP` is an empty native, so the story never loads this land |

## Freeing the creature (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: free the creature — the creature is held by three prison statues; each one stopped frees it a third | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md](gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md): the pillars follow stubbed town ownership |
| Lethys's possessed wolves attack the player's people; the monk explains and helps | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/the_wolves_are_possessed.md](gold_scrolls/the_wolves_are_possessed.md): started only when a pillar falls |
| Lethys sets sixteen of the Japanese village's fishermen alight at their beach campfire; they run burning for the village's buildings and must be put out before they get there | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/fire_fire_im_on_fire.md](gold_scrolls/fire_fire_im_on_fire.md): started only when a pillar falls; fire, villagers and spells are real |
| The water one-shot miracles the player is given to fight the fires | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; `CREATE` makes one-shot miracles; see ../miracles/water.md |
| Gold scroll: leave through the vortex — with the creature freed the player leaves for the fourth land | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/leave_through_the_vortex_land_3.md](gold_scrolls/leave_through_the_vortex_land_3.md) (its script has no stubs), the vortex in [portals.md](portals.md) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Shaolin" — a monk goes off to meditate in a secret place and asks not to be followed | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [silver_scrolls/the_shaolin.md](silver_scrolls/the_shaolin.md) |
| Silver scroll: "The Rejuvenator" — an old woman makes the old young; after three, her magic goes wrong on a child, who turns into an ape (or a chimp if the creature already is one) to swap for | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [silver_scrolls/the_rejuvenator.md](silver_scrolls/the_rejuvenator.md) |
| A bronze puzzle of trees with a reward | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; see [minigames.md](minigames.md) |
