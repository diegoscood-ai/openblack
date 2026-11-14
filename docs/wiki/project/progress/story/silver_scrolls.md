# Silver scrolls

The story's own quests, the gold scrolls, are listed in [gold_scrolls.md](gold_scrolls.md).

Every silver scroll of the five story lands, land by land, with links to each quest's own file in
[silver_scrolls/](silver_scrolls/). This is an index, not a feature file: it has no rows of its own and is not scored;
the numbers below are summed from the quest files.

Gold scrolls are the story: the player has to finish them to move on, and they are covered in the land files
([land_1.md](land_1.md) to [land_5.md](land_5.md)). Silver scrolls are the optional challenges: a silver scroll stands
over whoever asks for help, tapping it starts the quest, and the quest is kept in the challenge log with how well it went
and how good or evil the player was. How challenges and their rewards work in general is in
[challenges_and_rewards.md](challenges_and_rewards.md) and [rewards.md](rewards.md).

The list was drawn up from the game's challenge scripts: the 47 script files that put up a challenge highlight were
each checked against every land's control script to see whether, when and on which land the game starts them, and the
set that ships was taken from the compiled challenge program, so a quest that is only in the script sources is listed as
cut. Each table is in the order the land offers its quests, as far as the control script fixes it.

## Land 1

The land's control script starts the throwing lesson and the lost flock straight after the opening, the explorers and
the breeder once the creature is chosen, then the rest one by one as the creature's lessons go on.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [Throwing Stones](silver_scrolls/throwing_stones.md) | the advisors, over the hill above the pillar | a toy ball, then up to six water miracle seeds for practice throws | 47 | 36% |
| [The Lost Flock](silver_scrolls/the_lost_flock.md) | a Norse shepherd by the player's village | a large food reward at five sheep; a sheep creature to swap to at ten | 81 | 13% |
| [The Explorers](silver_scrolls/the_explorers.md) | three sailors by an unfinished ark on a beach | a water miracle dispenser | 89 | 10% |
| [The Immersion Mushrooms](silver_scrolls/the_immersion_mushrooms.md) | a hippy at his hut (only with a force-feedback mouse) | a compassion creature miracle dispenser, for the right mushroom | 38 | 9% |
| [The Singing Stones](silver_scrolls/the_singing_stones.md) | a hippy by the stone circle | a food miracle dispenser | 66 | 40% |
| [The Hermit](silver_scrolls/the_hermit.md) | a hermit at his hillside hut | a water miracle dispenser, or a water miracle from the sky if his hut was damaged | 88 | 10% |
| [The Saviour](silver_scrolls/the_saviour.md) | a fisherman's wife on the shore | a strength creature miracle dispenser, for saving all five or killing all five | 51 | 21% |
| [The Pied Piper](silver_scrolls/the_pied_piper.md) | the crèche woman of the Norse village | a heal miracle dispenser (good ending) or a lightning miracle dispenser (evil ending) | 85 | 16% |
| [The Ogre](silver_scrolls/the_ogre.md) | the advisors, at the ogre's pass | a beach ball, then a heal miracle dispenser by the temple | 53 | 6% |

The creature breeder (Land 1 and Land 4) is in the shared table below.

## Land 2

Khazar's land starts the singing stones and the beach temple at once. The others wait on towns: the control script
watches each quest's town from the start, and each quest opens when the player owns its town (the plague also waits for a
finished worship site, and so does the sacrifice; the sea needs two towns, the idol its town and more than five towns in
all).

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Singing Stones (land 2)](silver_scrolls/the_singing_stones_land_2.md) | a priest at the hut above the stones | none: each tune's effect (night with bats, the dead raised, snow) is the prize | 85 | 3% |
| [The Beach Temple Puzzle](silver_scrolls/the_beach_temple_puzzle.md) | a Greek farmer living in the beach temple | the temple heals every living thing within 10 of it, for good | 49 | 2% |
| [The Plague](silver_scrolls/the_plague.md) | a sick Indian trader of the poisoned village | the first lightning miracle enabled at the village | 56 | 5% |
| [The Riddles](silver_scrolls/the_riddles.md) | Annika, a woman of an Indian village | a zebra creature to swap to | 48 | 6% |
| [The Slavers](silver_scrolls/the_slavers.md) | a villager of the Greek village by Khazar, then the slavers' chief | a wolf-pack miracle dispenser, and the freed slaves and the slavers join the village | 104 | 9% |
| [The Sacrifice](silver_scrolls/the_sacrifice.md) | an Indian priest and priestess, at the player's worship site | a heal miracle chest, for reaching 4000 prayer power with the family alive | 71 | 4% |
| [The Sea](silver_scrolls/the_sea.md) | a mother at her hut in the Indian town by the bay | a "big" creature miracle dispenser, if all five sons walk home alive | 77 | 2% |
| [The Greedy Farmer](silver_scrolls/the_greedy_farmer.md) | a Celtic farmer on the edge of his town | the lightning miracle and its second level for the town | 143 | 2% |
| [The Idol](silver_scrolls/the_idol.md) | the good advisor, at the idol in the hills | the fireball miracle's first and second levels for the nearest town | 108 | 6% |

## Land 3

Lethys's land starts both quests as the player arrives through the vortex.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Rejuvenator](silver_scrolls/the_rejuvenator.md) | an old woman at a lone Celtic hut | an ape creature to swap to (a chimp if the creature already is an ape) | 57 | 4% |
| [The Shaolin](silver_scrolls/the_shaolin.md) | a guru at the mountain temple above the Japanese village | a Wonder begun for the player during the creature's rescue | 66 | 3% |

## Land 4

The ruined land starts the fish puzzle and the breeder at the beginning; the blind woman's quest waits until the player
owns the Aztec village.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md) | a boy turtle farmer on the shore | a tortoise creature to swap for | 43 | 7% |
| [The Treacherous Path](silver_scrolls/the_treacherous_path.md) | a woman at her hut in the Aztec village | a wolf creature to swap for, if her brother is healed | 60 | 5% |

## Land 5

Nemesis's land starts the wolf puzzle, the traitor and the crusaders at the beginning, and the returning explorers too if
their ark sailed on Land 1. The brown bear waits for the player to win the neutral Japanese village, the heavenly fire
for the first win of the Greek village.

| Quest | Giver | Reward | Rows | Score |
|-------|-------|--------|------|-------|
| [Stanley The Wolf](silver_scrolls/stanley_the_wolf.md) | the wolf's owner, by a campfire beside the puzzle | a lion creature to swap for, and the fireball's second and third levels | 52 | 5% |
| [The Japanese Traitor](silver_scrolls/the_japanese_traitor.md) | a stranger praying at a fire in the forest, at night | none: secrets about Nemesis's curse on the creature | 38 | 4% |
| [The Magic Dragon](silver_scrolls/the_magic_dragon.md) | the crusaders' leader, by the dragon's cave | a flying-flock miracle dispenser | 67 | 10% |
| [The Explorers Again](silver_scrolls/the_explorers_again.md) | three sailors by their wrecked boat | a polar bear to swap for, and a ready-built Norse town | 39 | 3% |
| [Swap To Brown Bear](silver_scrolls/swap_to_brown_bear.md) | a Japanese farmer on the edge of the neutral Japanese village | a brown bear to swap to, and a heal miracle chest | 53 | 3% |
| [The Heavenly Fire](silver_scrolls/the_heavenly_fire.md) | none: it starts by itself, announced by the evil advisor | an itchy creature miracle seed, if the village is well protected | 57 | 3% |

The heavenly fire puts up no scroll (that part of its script is switched off), but it is recorded in the challenge log
like the other silver quests, so it is counted here.

## On more than one land

| Quest | Lands | What it is | Rows | Score |
|-------|-------|------------|------|-------|
| [The Creature Breeder](silver_scrolls/the_creature_breeder.md) | 1 and 4 (also placed on 2 and 5 but never started there) | a standing silver scroll over the breeder's kennels, offering the special creatures to swap for; it comes back forever | 28 | 25% |
| [Creature swaps](silver_scrolls/creature_swaps.md) | all | not a quest: the shared offer that ends every quest whose reward is a creature, the breeder's stricter version of it, and the five swap scrolls cut before release | 38 | 12% |

## Cut or never started

Written for the game but never run by it, so they have no progress rows; what was found about each is on
[Not in the original](../../not-in-the-original.md) (the links below).

| Quest | Land | Rows | Why it never runs |
|-------|------|------|-------------------|
| [Swap To Cow](../../not-in-the-original.md#storysilver_scrollsswap_to_cow) | 4 | 31 | compiled into the shipped game, but no land's control script or any other script starts it |
| [Creature Ladder](../../not-in-the-original.md#storysilver_scrollscreature_ladder) | none | 24 | never compiled; it has no title or lines in the text table and its tattoo reward was never written |
| [The Big Whale](../../not-in-the-original.md#storysilver_scrollsbig_whale) | 1 | 19 | never compiled; none of its lines are in the text table |
| [The Sculptor (cut silver version)](../../not-in-the-original.md#storysilver_scrollsthe_sculptor_cut_silver) | 1 | 22 | never compiled; replaced by the gold-scroll Sculptor of the story, and even the early control script that names it could not start it |
| [The Nomads (Food for Thought)](../../not-in-the-original.md#storysilver_scrollsfood_for_thought) | 2 | 22 | never compiled; only its title is in the text table |
| [Landslide](../../not-in-the-original.md#storysilver_scrollslandslide) | 1 | 15 | never compiled; neither its title nor its lines are in the text table |
| [The Attackers](../../not-in-the-original.md#storysilver_scrollsthe_attackers) | 2 most likely | 66 | missing from the compiler's project list, so not in the compiled program; nothing starts it |
| [The Miracle Stones](../../not-in-the-original.md#storysilver_scrollsthe_miracle_stones) | 2 | 72 | missing from the compiler's project list, so not in the compiled program; nothing starts it |
| [See The Citadel](../../not-in-the-original.md#storysilver_scrollssee_the_citadel) | 1 | 41 | never compiled; an early draft of the opening at the temple, a tutorial rather than a silver scroll |
| [Chimp Posse](../../not-in-the-original.md#storysilver_scrollschimp_posse) | 5 | 43 | never compiled; it opens a challenge record but puts up no scroll and gives no reward |

Two more cut scrolls live inside other files: the five cut swap scrolls (horse, leopard, lion, tortoise, wolf) in
[creature_swaps.md](silver_scrolls/creature_swaps.md), and the Slavers Warning, a bronze scroll that is compiled but
never started, at the end of [the_slavers.md](silver_scrolls/the_slavers.md).

## Not silver scrolls

Some challenges look like silver scrolls but are not counted here:

- Lethys's vortex on Land 2 is logged as a step of the gold story, not as a silver quest (see [land_2.md](land_2.md)).
- Khazar's Fireball and Shield challenges on Land 2 are gold scrolls (see [land_2.md](land_2.md)).
- The bronze puzzles are signposted by information signs, not silver scrolls: the tree puzzles of lands 2 and 3, and the
  Theseus maze and the Japanese totem puzzle of land 4 (see [minigames.md](minigames.md)).
- The Spiritual Healer on Land 2 has no scroll, no highlight and no log entry; it is a village event, kept in
  [silver_scrolls/the_spiritual_healer.md](silver_scrolls/the_spiritual_healer.md) (63 rows, 3%) for completeness.
- Land 3's man who wants to be thrown ("throw the bloke") is a standing joke with no scroll or log entry (see
  [minigames.md](minigames.md#the-man-who-wants-to-be-thrown-land-3)).

## Totals

| Land | Silver quests | Rows | Score |
|------|---------------|------|-------|
| Land 1 | 9, plus the breeder | 598 | 17% |
| Land 2 | 9 | 741 | 4% |
| Land 3 | 2 | 123 | 3% |
| Land 4 | 2, plus the breeder | 103 | 6% |
| Land 5 | 6 | 306 | 5% |
| The breeder (lands 1 and 4) | 1 | 28 | 25% |
| All lands | 29 | 1899 | 9% |

The all-lands line counts the 28 single-land quests and the breeder once: 12 done, 369 partial and 1518 pending
rows (no n/a rows are left). The shared swap offer (38 rows, 12%), the Spiritual Healer and the cut quests are left out of it.

## Known openblack blockers

None of the silver quests runs to its end in openblack yet, for the same few reasons:

- Only Land 1's control script runs: it starts the singing stones at once, the throwing lesson and the lost flock after
  the opening; in a run of 2026-10-22 a game that does not skip the training plays the opening and The Temple Is
  Finished and lays the first gate stone, but has not gone further, so the rest starts only in a game that skips the
  creature training. The control scripts of Lands 2 to 5 run only after the earlier lands' exits, because the story
  always begins with Land 1; the land-loading command changes land now, but no check has reached them yet.
- Scripts make creatures and reward chests now (`CREATE`, `src/CHLApi.cpp`), as well as villagers, animals, scenery,
  rocks, dispensers and puzzles.
- An object's health and whether the player's hand holds it are read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`,
  `GetProperty` in `src/CHLApi.cpp`); the quests that took their giver for dead at once while health answered 0 are not yet
  re-checked in play.
- Stopping a script by name compares the whole text with each script's name. The game is reported to split the text
  into several names at spaces, commas and tabs; this comes from the executable and has not been checked yet.
- The challenge log, the rewards, the creature swap and the held-object checks are real. A creature action that a
  script forces (`CREATURE_DO_ACTION`) is planned through the creature's mind by its row's name; the rows that had no
  executor (HoldObject, PutDown, PointAtObject, SleepByObject and DeadForever) have one since 2026-10-18 and 2026-10-20
  (`src/Creature/CreaturePlanActions.cpp`), so The Pied Piper's catch no longer lacks its actions, but it is not yet
  re-checked in play. Dialogue, advisors, camera moves, highlights and timers work.
