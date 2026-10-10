# Land 2

Khazar's land: the player arrives through the vortex to find the friendly god Khazar and the hostile Lethys. Khazar
teaches worship and miracles, the player wins over the land's towns, and Lethys ends the land by stealing the creature.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/27 done, 0 partial — 0%**

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land loads and its control script begins | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; `LandControlAll` runs `LandControl1` first whatever land is loaded (`Game.cpp`). The land script's contents: see ../scripts/land2_script.md |
| The player's people come out of the vortex with the creature and found a new town | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; the arrival vortex's emission is ported (see [portals.md](portals.md)), the script around it is not reached |
| Khazar introduces himself, gives the first scaffold and the first one-shot miracles | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; Khazar's hand is a computer player, all of whose natives are stubs |
| Lethys shows himself with a fake fireball attack and his computer player starts | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; the computer player natives are stubs |
| The land's eleven towns are watched and the next challenges open as the player wins them over | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; the town ownership reads (`GetTownWithId`, owners) are stubs |
| Khazar is the player's ally and Lethys an enemy god with his own temple and creature | todo | the rival gods' computer players and creatures are stubs (`LoadCreature`, all computer-player natives); see ../multiplayer/player_diplomacy.md |

## Khazar's lessons (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "Worship Site" — Khazar shows the player the worship site, raising and lowering the village totem, and how worshippers make prayer power | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/worship_site.md](gold_scrolls/worship_site.md): many natives are real, but it would hang at the first wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Gold scroll: "Impress Village" — Khazar teaches casting a miracle by gesture without going back to the temple, and impressing a village with it | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/impress_village.md](gold_scrolls/impress_village.md): the store reads and Khazar's hand are stubs |
| Gold scroll: "Khazar's Fireball Challenge" — three huts as targets and fireball seeds to throw at them | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/khazars_fireball_challenge.md](gold_scrolls/khazars_fireball_challenge.md): Khazar's hand, the held object and building health are stubs or unhandled |
| Gold scroll: "Khazar's Shield Challenge" — Khazar shows a Physical Shield at a lone hut on an island, hands over three shield seeds, and drops three boulders of his own on the hut the player has shielded | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/khazars_shield_challenge.md](gold_scrolls/khazars_shield_challenge.md): Khazar's hand, the hit test and building health are stubs or unhandled |
| Khazar's lesson on influence: grow towards Lethys | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; covered in [gold_scrolls/impress_village.md](gold_scrolls/impress_village.md#khazars-lesson-on-influence); virtual influence (`SetVirtualInfluence`) is a stub |
| Gold scroll: "The Workshop" — a man sent by Khazar shows the workshop and how scaffolds plan new buildings | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/the_workshop.md](gold_scrolls/the_workshop.md): the workshop's wood read (`GetResource`) is a stub |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Sea" — a woman's children have swum out too far; save them (or don't) | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_sea.md](silver_scrolls/the_sea.md) |
| Silver scroll: "The Beach Temple Puzzle" — move a flooding temple's rings up the beach column by column, a ring never on a smaller one | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_beach_temple_puzzle.md](silver_scrolls/the_beach_temple_puzzle.md) |
| Silver scroll: "The Greedy Farmer" — a farmer's cows are stolen by children; what the player does to the thieves and the farmer sets the alignment | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_greedy_farmer.md](silver_scrolls/the_greedy_farmer.md) |
| Silver scroll: "The Idol" — a man has built an idol to worship; burn it, or the people, or leave it | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_idol.md](silver_scrolls/the_idol.md) |
| Silver scroll: "The Riddles" — a woman's riddles ask for things put in a stone ring ("something which howls at night", "something hot" …) | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_riddles.md](silver_scrolls/the_riddles.md) |
| Silver scroll: "The Plague" — Lethys poisons a village; heal it and stop the plague spreading | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_plague.md](silver_scrolls/the_plague.md) |
| Silver scroll: "The Sacrifice" — a tribe's altar turns anything laid on it into prayer power; what is sacrificed sets the alignment | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_sacrifice.md](silver_scrolls/the_sacrifice.md) |
| Silver scroll (no title in the game's text): a spiritual healer in a village and what the player lets him do | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_spiritual_healer.md](silver_scrolls/the_spiritual_healer.md) |
| Silver scroll: "The Slavers" — strangers who want wild animals for a circus are holding kidnapped villagers; a villager first warns of them | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_slavers.md](silver_scrolls/the_slavers.md) |
| Silver scroll: "The Singing Stones" — stones that each have a voice; playing their melodies wakes the spirits of the ancients | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [silver_scrolls/the_singing_stones_land_2.md](silver_scrolls/the_singing_stones_land_2.md) |
| A bronze puzzle of trees with a miracle dispenser as its prize | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; its rules and prize are in [minigames.md](minigames.md) (Tree puzzles) |

## The end of the land (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar is killed by Nemesis | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/nemesis_no.md](gold_scrolls/nemesis_no.md): the trigger reads stubbed town ownership and the rival creatures are never made |
| Gold scroll, the land's last: win or wipe out Lethys's three towns | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/destroy_it.md](gold_scrolls/destroy_it.md): the town counts are stubs |
| Lethys takes the creature away through a vortex and the advisors despair | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/lethys_has_taken_our_creature.md](gold_scrolls/lethys_has_taken_our_creature.md): walking the creature by script and Lethys's hand are not done |
| Gold scroll: leave through the vortex — the player follows Lethys into the third land | todo | never reached: `LOAD_MAP` is an empty native, so the story never loads this land; every step in [gold_scrolls/leave_through_the_vortex_land_2.md](gold_scrolls/leave_through_the_vortex_land_2.md), the vortex in [portals.md](portals.md); its trigger reads stubbed town totals |
