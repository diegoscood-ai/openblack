# Land 5

Nemesis's land: Nemesis curses the creature, his towns defend themselves with shields, and the player takes his land town
by town until the last fight between the creatures and the end of the game.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/13 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land5_script.md |
| Nemesis greets the player with "a surprise": the creature is cursed | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): creature actions by script go through the mind (`CreatureDoAction`, partly); reading the creature's height (`GetProperty` 19) is real (fifteen times its drawn size) |
| The curse warps the creature's strength, size and alignment | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): setting a creature's strength and size and reading its height (`GetProperty` 19) are real; see ../creature/ |
| Lifting the curse in three steps puts the creature back as it was | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): the village captures are read through real natives |
| The volcano rumbles | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the volcano vortex is ported (`ecs::vortex`, Volcano) |
| Nemesis defends his last town himself | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md); the meteors, shields and blasts use real natives, and so is the aim, `SetTarget` is real (not seen in play yet) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Magic Dragon" — crusaders want healing before they fight the dragon in its cave | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md) |
| Silver scroll: "The Heavenly Fire" — meteors rain on a village; catch them before they hit | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/the_heavenly_fire.md](silver_scrolls/the_heavenly_fire.md); burning rocks: see ../nature/rocks_splitting_and_heat.md |
| Silver scroll: "Stanley The Wolf" — a blind wolf walks towards ringing bells; ring them to lead it to the sheep | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Silver scroll: "The Explorers Again" — the missionaries of the first land arrive in their boat | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/the_explorers_again.md](silver_scrolls/the_explorers_again.md); it only starts if the ark sailed in [The Explorers](silver_scrolls/the_explorers.md) |
| Silver scroll: "Swap To Brown Bear" — a stench in the forest; clearing what causes it offers a brown bear to swap for | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/swap_to_brown_bear.md](silver_scrolls/swap_to_brown_bear.md) |
| Silver scroll (no title in the game's text): someone hiding in the forest is plotting against a village | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [silver_scrolls/the_japanese_traitor.md](silver_scrolls/the_japanese_traitor.md) |
| A village behind Nemesis's spiritual shield must be broken through | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md): `BeliefForPlayer` answers for towns; `SetMagicRadius` and `IsAffectedBySpell` are stubs |

## The end

Winning Nemesis's last towns, his "not finished" and the big fight between the creatures are rows of [ending.md](ending.md) (Nemesis's defeat).
