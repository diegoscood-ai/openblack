# Challenges and rewards

How a challenge runs across the lands: it is found by its scroll, recorded when started and finished with how well it
went and how good or evil the player was, and pays out a reward. The scrolls themselves are in
../interface/scrolls_and_signs.md.

Every silver scroll, land by land, is listed in [silver_scrolls.md](silver_scrolls.md); the shared creature swap is in
[silver_scrolls/creature_swaps.md](silver_scrolls/creature_swaps.md) and the breeder in
[silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md).

**Progress: 10/23 done, 0 chosen, 5 partial, 0 inferred — 57%**

## Running a challenge

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge starts when its scroll is tapped and the advisors announce it | pending | the scroll (`CreateHighlight`), the click (`GameThingClicked`) and `ChallengeHighlightNotify` with the advisors are real; most challenges are not yet reached (Land 1 has not yet been run past the creature glade without skipping the training since `CREATE` of a creature became real, and the story cannot leave Land 2); see ../interface/scrolls_and_signs.md |
| Gold scrolls are the story and must be done; silver ones are optional | pending | gold and silver scrolls are highlights of their kind (`src/ECS/ScriptHighlight.cpp`); which ones the story must finish is the scripts', mostly never reached; every silver scroll: [silver_scrolls.md](silver_scrolls.md) |
| Bronze "did you know" scrolls give tips and some hide puzzles | partial 60% | the did-you-know scrolls are placed and read (`DidYouKnow` script, the bubble in `Help/HelpSystem`, `test/test_did_you_know_bubble.cpp`); the puzzles they hide are mostly on later lands; re-measured 2026-10-21: placed and read; the puzzles they hide are mostly on later lands |
| A challenge can be set to wait until another is finished | pending | the scripts' own waits run in our script machine; most challenges are never reached; e.g. [The Ogre](silver_scrolls/the_ogre.md) waits for the guide's fight lesson, and [The Shaolin](silver_scrolls/the_shaolin.md)'s Wonder comes only if it was done before the creature's rescue |
| A started challenge is recorded with a title, a reminder and a picture | done | `Snapshot` in `src/CHLApi.cpp` keeps the record (`ecs::challenge_records`) and asks for its picture (`ecs::challenge_pictures`); unverified against v1.42 |
| Its record is updated with how well it went (0 to 1) and the alignment it earned | done | `UpdateSnapshot` in `src/CHLApi.cpp` updates the record (`ecs::challenge_records`); unverified against v1.42 |
| Tapping the scroll again replays the challenge's reminder | done | `InterfaceTap` runs the record's reminder script with its arguments (`CHLApi::StartChallengeReminder`); the reminder scripts call only real natives; unverified against v1.42; see ../interface/scrolls_and_signs.md |
| The challenge room shows every recorded challenge | done | the room's scroll and pictures are filled from the records (`ecs::challenge_records::List`, `src/3D/Implementations/TempleInterior.cpp`); unverified against v1.42; see ../temple/challenge_room.md |
| A challenge can be replayed from the challenge room | done | from its picture or its button (`TempleInterior::ClickChallenge`, `CHLApi::StartChallengeScript`); unverified against v1.42; see ../temple/challenge_room.md |
| What the player did in a challenge moves their alignment | done | a snapshot of a challenge already recorded moves the local player's alignment at once (`KeepChallengeRecord`, `src/CHLApi.cpp`); unverified against v1.42; see ../worship/alignment.md |
| Each time a challenge's record is updated, its alignment (−1 to 1) moves the player's alignment, scaled by a factor of the player's own | done | `UpdateSnapshot` moves it by `challenge_records::AlignmentChange`: the record's alignment scaled by the game's most a script may move it (`maxScriptAlignmentChange`), damped by the player's own alignment; unverified against v1.42; seen in [The Pied Piper](silver_scrolls/the_pied_piper.md) and [The Sea](silver_scrolls/the_sea.md); see ../worship/alignment.md |

## Rewards

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reward chest falls from the sky with a trail of dust | done | `CreateReward` gives the chest, which falls with its dust ([rewards.md](rewards.md)); unverified against v1.42; e.g. the toy ball of [Throwing Stones](silver_scrolls/throwing_stones.md) |
| A reward can appear in a town, or without falling | done | `CreateRewardInTown` and `CreateReward` in `src/CHLApi.cpp`, from the sky or on the land ([rewards.md](rewards.md)); unverified against v1.42 |
| Opening a reward chest gives what it holds | done | tapped open, the chest gives what it holds (`RewardSystem::Tap`, `reward::Open`, [rewards.md](rewards.md)); unverified against v1.42; e.g. the large food reward of [The Lost Flock](silver_scrolls/the_lost_flock.md) |
| The first reward is explained by the advisors | partial 60% | the first-reward help scripts read the chest's help text through `GetHelp` (`src/CHLApi.cpp`), and the chest is made; not checked in play against the original; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| A miracle dispenser as a reward, with how many charges and how often it refills | partial 70% | a script can make a dispenser (`CREATE` of a spell dispenser, `magic::script::CreateSpellDispenser`, `src/Worship/SpellDispenser.cpp`); the reward script's help lines need `GetFirstHelp` and `GetLastHelp`, real for a spell dispenser; see ../miracles/dispensers_and_seeds.md, every dispenser reward in [rewards.md](rewards.md); re-measured 2026-10-21: a script makes it; its help lines' natives are real |
| One-shot miracle seeds as a reward | partial 70% | a script can make a one-shot seed (`CREATE` of a one-shot spell, `magic::script::CreateOneShotSpell`); the chest that holds one is made and opens now (`CreateReward`); see ../miracles/dispensers_and_seeds.md; re-measured 2026-10-21: the seed and its chest are made and open |
| A new creature to swap for | pending | `SwapCreature` is real; see ../creature/; the shared offer: [creature_swaps.md](silver_scrolls/creature_swaps.md); e.g. the sheep of [The Lost Flock](silver_scrolls/the_lost_flock.md), the tortoise of [The Fish Puzzle](silver_scrolls/the_fish_puzzle.md), the wolf of [The Treacherous Path](silver_scrolls/the_treacherous_path.md) |
| Gate stones and keys that open the way | pending | the gate stones are made by the Land 1 scripts and the plinth reading (`ObjectInfoBits`) is real; in the 2026-10-22 Land 1 runs (a new game with no skip, fixed time step) laying the tiger gate stone in its plinth set `GateKey2Present` to 1 and started The Lost Brother; the other two stones are not reached yet, and behind the gate the glade's creatures are made now (`CREATE` of a creature); see [land_1.md](land_1.md) |
| Rewards are signposted with a bronze scroll | partial 50% | the bronze scroll is a script highlight (`CreateHighlight`); no reward is made to sign; see ../interface/scrolls_and_signs.md; re-measured 2026-10-21: the scroll is a highlight; no reward is made to sign |

## Creature swaps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The breeder offers creatures; the player confirms with the Action button on the one they want | pending | `SwapCreature` is real (`src/CHLApi.cpp`: the two creatures' minds change bodies, `CreatureMindSystem::SwapMinds`), and the breeder's offer is ported (`src/Creature/CreatureSwap.h`); not checked in play; see [the_creature_breeder.md](silver_scrolls/the_creature_breeder.md) |
| The new creature keeps what the old one learnt | pending | `SwapMinds` moves what it learnt, how it feels, its physique and its tattoos with the mind; each body keeps its name and species and eases to the other's size and alignment (`src/Creature/CreatureSwap.h`); not checked against the original in play |
| Swap to an ape, a cow, a brown bear and other species through the story | pending | `SwapCreature` is real; no run has reached a swap scroll yet; the cut swap scrolls: [creature_swaps.md](silver_scrolls/creature_swaps.md#cut-swap-scrolls-never-in-the-game) |
