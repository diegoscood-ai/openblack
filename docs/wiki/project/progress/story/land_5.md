# Land 5

Nemesis's land: Nemesis curses the creature, his towns defend themselves with shields, and the player takes his land town
by town until the last fight between the creatures and the end of the game.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/13 done, 0 chosen, 2 partial, 0 inferred — 9%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | pending | not yet reached in play: runs started at the land's setup and control scripts load Land 5 and run it (refs/story/evidence/a2l5_control.log, a2l5_surprise.log), but none comes to it from Land 4 through the vortex; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land5_script.md |
| Nemesis greets the player with "a surprise": the creature is cursed | partial 60% | quest_check PASS, run a2l5_surprise (land5, 2026-10-30 23:33, refs/story/evidence/a2l5_surprise.log line 36507; also a2l5_surprise2.log line 36508), a quest run of the land's setup and control scripts (quest.i_have_a_surprise), spec tools/checks/story/quests/land_5.txt: Nemesis's line by the alignment and the curse shown on the creature; the land change from Land 4 is not in this run; finished by [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md) (rows still pending and partial there); full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): creature actions by script go through the mind (`CreatureDoAction`, partly); reading the creature's height (`GetProperty` 19) is real (fifteen times its drawn size) |
| The curse warps the creature's strength, size and alignment | partial 60% | quest_check PASS, run a2l5_surprise (land5, 2026-10-30 23:40, refs/story/evidence/a2l5_surprise.log line 562010; also a2l5_surprise2.log line 562026), a quest run of the land's setup and control scripts (quest.i_have_a_surprise), spec tools/checks/story/quests/land_5.txt: the strength, size and alignment warps each run once; finished by [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md) (rows still pending and partial there); full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): setting a creature's strength and size and reading its height (`GetProperty` 19) are real; see ../creature/ |
| Lifting the curse in three steps puts the creature back as it was | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [gold_scrolls/i_have_a_surprise_for_you.md](gold_scrolls/i_have_a_surprise_for_you.md): the village captures are read through real natives |
| The volcano rumbles | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; the volcano vortex is ported (`ecs::vortex`, Volcano) |
| Nemesis defends his last town himself | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md); the meteors, shields and blasts use real natives, and so is the aim, `SetTarget` is real (not seen in play yet) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Magic Dragon" — crusaders want healing before they fight the dragon in its cave | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md) |
| Silver scroll: "The Heavenly Fire" — meteors rain on a village; catch them before they hit | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/the_heavenly_fire.md](silver_scrolls/the_heavenly_fire.md); burning rocks: see ../nature/rocks_splitting_and_heat.md |
| Silver scroll: "Stanley The Wolf" — a blind wolf walks towards ringing bells; ring them to lead it to the sheep | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Silver scroll: "The Explorers Again" — the missionaries of the first land arrive in their boat | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/the_explorers_again.md](silver_scrolls/the_explorers_again.md); it only starts if the ark sailed in [The Explorers](silver_scrolls/the_explorers.md) |
| Silver scroll: "Swap To Brown Bear" — a stench in the forest; clearing what causes it offers a brown bear to swap for | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/swap_to_brown_bear.md](silver_scrolls/swap_to_brown_bear.md) |
| Silver scroll (no title in the game's text): someone hiding in the forest is plotting against a village | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [silver_scrolls/the_japanese_traitor.md](silver_scrolls/the_japanese_traitor.md) |
| A village behind Nemesis's spiritual shield must be broken through | pending | not yet reached in play: the natural story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log end on Land 3; full breakdown: [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md): `BeliefForPlayer` answers for towns; `SetMagicRadius` and `IsAffectedBySpell` are stubs |

## The end

Winning Nemesis's last towns, his "not finished" and the big fight between the creatures are rows of [ending.md](ending.md) (Nemesis's defeat).
