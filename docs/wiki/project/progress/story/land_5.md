# Land 5

Nemesis's land: Nemesis curses the creature, his towns defend themselves with shields, and the player takes his land town
by town until the last fight between the creatures and the end of the game.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/15 done, 0 partial — 0%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land5_script.md |
| Nemesis greets the player with "a surprise": the creature is cursed | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): creature actions by script are stubs |
| The curse warps the creature's strength, size and alignment | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): a creature's strength and size are not handled by `SetProperty`; see ../creature/ |
| Lifting the curse in three steps puts the creature back as it was | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): the village captures read stubs |
| The volcano rumbles | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; the volcano vortex is ported (`ecs::vortex`, Volcano) |
| Nemesis defends his last town himself | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md); the meteors, shields and blasts use real natives, the aim (`SetTarget`) is a stub |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Magic Dragon" — crusaders want healing before they fight the dragon in its cave | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md) |
| Silver scroll: "The Heavenly Fire" — meteors rain on a village; catch them before they hit | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/the_heavenly_fire.md](silver_scrolls/the_heavenly_fire.md); burning rocks: see ../nature/rocks_splitting_and_heat.md |
| Silver scroll: "Stanley The Wolf" — a blind wolf walks towards ringing bells; ring them to lead it to the sheep | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Silver scroll: "The Explorers Again" — the missionaries of the first land arrive in their boat | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/the_explorers_again.md](silver_scrolls/the_explorers_again.md); it only starts if the ark sailed in [The Explorers](silver_scrolls/the_explorers.md) |
| Silver scroll: "Swap To Brown Bear" — a stench in the forest; clearing what causes it offers a brown bear to swap for | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/swap_to_brown_bear.md](silver_scrolls/swap_to_brown_bear.md) |
| Silver scroll (no title in the game's text): someone hiding in the forest is plotting against a village | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [silver_scrolls/the_japanese_traitor.md](silver_scrolls/the_japanese_traitor.md) |
| Cut challenge: "Chimp Posse" — three chimp creatures on a hilltop have lost their ball to Nemesis's creature | n/a | never compiled into the game, no scroll and no reward: [silver_scrolls/chimp_posse.md](silver_scrolls/chimp_posse.md) |
| A village behind Nemesis's spiritual shield must be broken through | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md): `BeliefForPlayer` and `SetMagicRadius` are stubs |

## The end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All of Nemesis's towns are won and he says he is not finished | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md); see ending.md |
| The big fight between the player's creature and Nemesis's | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md): the mirror creature is never made (`LoadCreature`); see ending.md and ../creature/fighting.md |
