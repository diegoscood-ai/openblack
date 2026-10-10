# Challenges and rewards

How a challenge runs across the lands: it is found by its scroll, recorded when started and finished with how well it
went and how good or evil the player was, and pays out a reward. The scrolls themselves are in
../interface/scrolls_and_signs.md.

Every silver scroll, land by land, is listed in [silver_scrolls.md](silver_scrolls.md); the shared creature swap is in
[silver_scrolls/creature_swaps.md](silver_scrolls/creature_swaps.md) and the breeder in
[silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md).

**Progress: 0/23 done, 5 partial — 11%**

## Running a challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge starts when its scroll is tapped and the advisors announce it | todo | the scroll (`CreateHighlight`), the click (`GameThingClicked`) and `ChallengeHighlightNotify` with the advisors are real; most challenges are never reached (Land 1 stops at the creature gate, Lands 2-5 need `LOAD_MAP`); see ../interface/scrolls_and_signs.md |
| Gold scrolls are the story and must be done; silver ones are optional | todo | gold and silver scrolls are highlights of their kind (`src/ECS/ScriptHighlight.cpp`); which ones the story must finish is the scripts', mostly never reached; every silver scroll: [silver_scrolls.md](silver_scrolls.md) |
| Bronze "did you know" scrolls give tips and some hide puzzles | partial | the did-you-know scrolls are placed and read (`DidYouKnow` script, the bubble in `Help/HelpSystem`, `test/test_did_you_know_bubble.cpp`); the puzzles they hide are mostly on later lands |
| A challenge can be set to wait until another is finished | todo | the scripts' own waits run in our script machine; most challenges are never reached; e.g. [The Ogre](silver_scrolls/the_ogre.md) waits for the guide's fight lesson, and [The Shaolin](silver_scrolls/the_shaolin.md)'s Wonder comes only if it was done before the creature's rescue |
| A started challenge is recorded with a title, a reminder and a picture | todo | `Snapshot` is a stub in `src/CHLApi.cpp`: no challenge is recorded |
| Its record is updated with how well it went (0 to 1) and the alignment it earned | todo | `UpdateSnapshot` is a stub in `src/CHLApi.cpp` |
| Tapping the scroll again replays the challenge's reminder | partial | the reminder scripts call only real natives; the challenge log behind it is not made (`Snapshot` stub); see ../interface/scrolls_and_signs.md |
| The challenge room shows every recorded challenge | todo | the temple's rooms exist (`src/3D/TempleScrolls.cpp`, `src/3D/TempleSigns.cpp`), but no challenge is ever recorded; see ../temple/challenge_room.md |
| A challenge can be replayed from the challenge room | todo | no recorded challenges; see ../temple/challenge_room.md |
| What the player did in a challenge moves their alignment | todo | `UpdateSnapshot`, which carries the alignment, is a stub; see ../worship/alignment.md |
| Each time a challenge's record is updated, its alignment (−1 to 1) moves the player's alignment, scaled by a factor of the player's own | todo | `UpdateSnapshot` is a stub; the factor's value is not determined; seen in [The Pied Piper](silver_scrolls/the_pied_piper.md) and [The Sea](silver_scrolls/the_sea.md); see ../worship/alignment.md |

## Rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest falls from the sky with a trail of dust | todo | `CreateReward` is a stub and our tree has no reward chest; e.g. the toy ball of [Throwing Stones](silver_scrolls/throwing_stones.md) |
| A reward can appear in a town, or without falling | todo | `CreateRewardInTown` and `CreateReward` are stubs in `src/CHLApi.cpp` |
| Opening a reward chest gives what it holds | todo | no reward chest in our tree; e.g. the large food reward of [The Lost Flock](silver_scrolls/the_lost_flock.md) |
| The first reward is explained by the advisors | todo | the first-reward help scripts wait on `GetHelp`, a stub, and no chest is made |
| A miracle dispenser as a reward, with how many charges and how often it refills | partial | a script can make a dispenser (`CREATE` of a spell dispenser, `magic::script::CreateSpellDispenser`, `src/Worship/SpellDispenser.cpp`); the reward script's help lines need `GetFirstHelp` and `GetLastHelp`, stubs; see ../miracles/dispensers_and_seeds.md, every dispenser reward in [rewards.md](rewards.md) |
| One-shot miracle seeds as a reward | partial | a script can make a one-shot seed (`CREATE` of a one-shot spell, `magic::script::CreateOneShotSpell`); the chest that would hold one is not made (`CreateReward` stub); see ../miracles/dispensers_and_seeds.md |
| A new creature to swap for | todo | `SwapCreature` is a stub; see ../creature/; the shared offer: [creature_swaps.md](silver_scrolls/creature_swaps.md); e.g. the sheep of [The Lost Flock](silver_scrolls/the_lost_flock.md), the tortoise of [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md), the wolf of [The Treacherous Path](silver_scrolls/the_treacherous_path.md) |
| Gate stones and keys that open the way | todo | the gate stones are made by the Land 1 scripts, but the plinth reading (`ObjectInfoBits`) is a stub, so they never open the gate; see [land_1.md](land_1.md) |
| Rewards are signposted with a bronze scroll | partial | the bronze scroll is a script highlight (`CreateHighlight`); no reward is made to sign; see ../interface/scrolls_and_signs.md |

## Creature swaps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The breeder offers creatures; the player confirms with the Action button on the one they want | todo | `SwapCreature` and the creature commands are stubs in `src/CHLApi.cpp`; see [the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |
| The new creature keeps what the old one learnt | todo | no creature swap in our tree (unconfirmed which parts carry over) |
| Swap to an ape, a cow, a brown bear and other species through the story | todo | no creature swap in our tree; the cut swap scrolls: [creature_swaps.md](silver_scrolls/creature_swaps.md#cut-swap-scrolls-never-in-the-game) |
