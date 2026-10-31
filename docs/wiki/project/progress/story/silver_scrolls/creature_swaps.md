# Creature swaps

The shared offer that ends every quest whose reward is a new creature: a creature appears under a silver scroll, the
player brings their own creature to it and clicks twice, and the creature's mind moves into the new body. A second,
stricter version runs the creature breeder's kennels. This file also covers the five swap scrolls that were written but
cut before release (horse, leopard, lion, tortoise, wolf).

**Land:** all (run by quests on lands 1 to 5) · **Giver:** the quest that ends in a creature · **Script:** SwapCreatures, SwapCreaturesAtBreeder · **Reward:** the new species for the player's creature · **Repeatable:** yes (the field offer never ends)

**Progress: 0/36 done, 0 chosen, 5 partial, 0 inferred — 8%**

Sources: the swap script and every script that runs it (the original source text, matching the PC game's compiled
`challenge.chl`), the game's text table and the executable's swap command. openblack's state is judged on this tree:
`SwapCreature` and `IsCreatureAvailable` are ported (raffclar PR #384, unverified against v1.42); the creature-control
commands the offer needs are real now (`MOVE_GAME_THING` and `SET_FOCUS` on a creature, `CREATURE_DO_ACTION`, which runs
a row that has an executor: looking at the camera (82), examining by looking (20), idling (161) and pointing at an
object (163) have one, `src/Creature/CreaturePlanActions.cpp`), its dialogue, highlight and special-effect commands
work, and `CREATE` makes creatures now, though no offer has yet been checked in play; the swap is reached through the testbed scenario
`mind.creature_swap` and the creature spawner's "Swap minds with the player's creature".

## Who offers a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1, "The Lost Flock": a sheep creature, once the whole flock is penned | pending | The Lost Flock starts with the fourth tutorial answer, but `CREATE` makes creatures now; the sheep creature's offer is not checked in play |
| Land 2, "The Riddles": a zebra appears on the altar | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 3, "The Rejuvenator": an ape (a chimp when the player's creature is already an ape) | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 4, "The Treacherous Path" (the blind woman): a wolf | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 4, "The Fish Puzzle": a tortoise | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 5, "Stanley The Wolf": a lion (the tamer can't die, so it is always offered) | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 5, "The Explorers Again": a polar bear | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| Land 5, "Swap To Brown Bear": a brown bear | pending | the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real, and `CREATE` makes creatures now |
| The creature breeder on lands 1 and 4: the five special creatures and the player's previous creature | partial 60% | ported from raffclar PR #384, unverified against v1.42: `IS_CREATURE_AVAILABLE` answers yes for every species, the five bonus creatures (Leopard, Horse, Mandrill, Rhino, Gorilla) included, as the original (`creature_swap::k_GameUnlocks`, checked in the exe 2026-10-19); the breeder's own offer is not checked; finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Whether a species of creature is available to choose", [species_choice.md](../../creature/species_choice.md) "Silver scroll challenges in the lands offer other species"; still open: no quest run's PASS is cited for it |

## The offer in the field

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The offered creature can't be picked up while it is on offer | pending | `CREATE` makes the offered creature now; not checked in play; `SET_ID_PICKUPABLE` and `CREATE_HIGHLIGHT` themselves work |
| A silver scroll appears over the offered creature, one unit above its height | pending | `CREATE` makes the offered creature now; not checked in play; `SET_ID_PICKUPABLE` and `CREATE_HIGHLIGHT` themselves work |
| Until clicked, the creature keeps turning to face the camera and plays its look-at-camera action | pending | `CREATE` makes the offered creature now; not checked in play; `SET_FOCUS` on a creature is real (`SetFocus` in `src/CHLApi.cpp`) and the look-at-camera action (row 82) has an executor |
| While the camera is within 100 of the scroll and the creature is on screen, the evil advisor steps out at most once a minute, points at the scroll and says "Swap your Creature with this one if you want." | pending | not checked in play (`CREATE` makes the offered creature now); the notice script, `GAME_THING_CLICKED` and the advisors work |
| Clicking the scroll or the creature itself starts the swap | pending | not checked in play (`CREATE` makes the offered creature now); the notice script, `GAME_THING_CLICKED` and the advisors work |
| If the player's creature is not within 50 of it, the evil advisor says "You'll need to bring our Creature, Boss." and the scroll comes back for another click | pending | not checked in play (`CREATE` makes the offered creature now); the notice script, `GAME_THING_CLICKED` and the advisors work |
| Otherwise the scroll goes, the leash is taken off and the player's creature is walked to within 15 of the new one; the game waits up to 5 seconds for it to get within 20 | pending | `CREATE` makes the offered creature now; not checked in play; moving and turning a creature from a script are real (`MOVE_GAME_THING` walks it, `CreatureMindSystem::MoveForScript`; `SET_FOCUS` turns it) |
| Both creatures turn and examine each other by looking | pending | `CREATE` makes the offered creature now; not checked in play; `SET_FOCUS` on a creature is real and examining by looking (row 20) has an executor |
| The good advisor asks, every 30 seconds until answered: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel." (the last token is replaced by the action-button picture) | pending | not checked in play (`CREATE` makes the offered creature now); the timer and `RUN_TEXT` work |
| Clicking the new creature confirms; clicking the player's own creature cancels and frees it; after 110 seconds with no answer the offer cancels itself | pending | not checked in play (`CREATE` makes the offered creature now); the timer and `RUN_TEXT` work |
| A cancelled offer puts the scroll back over the new creature and starts again from the first click | pending | not checked in play (`CREATE` makes the offered creature now); the timer and `RUN_TEXT` work |
| A random 60 to 120 second offer timer is made but never checked in the field offer, so the offer never runs out: a quest's offered creature keeps its silver scroll for the rest of the land, even after the player has swapped | pending | not checked in play (`CREATE` makes the offered creature now); the timer and `RUN_TEXT` work |

## The swap itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A short cut scene with a two-subject camera framing both creatures, which face each other | pending | not checked in play (`CREATE` makes the offered creature now); `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work |
| After a second, a spell-success sparkle appears on each creature, the leash comes off again and they point at each other | pending | not checked in play; `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work; pointing at an object (`CREATURE_DO_ACTION` row 163 PointAtObject) has an executor now (`Build::PointAtThing`, `src/Creature/CreaturePlanActions.cpp`) |
| The swap sound plays as the bodies are swapped, and the cut scene waits until both have finished their animations, then 3 more seconds | pending | not checked in play (`CREATE` makes the offered creature now); `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work |
| The new body takes on the player's creature's mind: everything it has learnt, its feelings, its physique (strength, fatness, needs) and its tattoos move with it. Each body keeps its own name, its own size and its own alignment; over the swap each grows or shrinks to the other's size and takes on the other's alignment as they were before any spell, 4% of the gap a turn, until both gaps are under 0.01 | partial 60% | no swap run in play yet (no quest_check PASS), so the code and tests below are the measured share; ported from raffclar PR #384, unverified against v1.42: `CreatureMindSystem::SwapMinds` (`ECS/Systems/Implementations/CreatureMindSwap.cpp`: `player_creature::ToMindFile`, `TakeUpFile`, `player_creature::TakeUpBody`, the own name put back with `player_creature::SetName`), the idle step `SwapMind` and `creature_swap::Ease` / `IsSettled`; `CreatureSwap.*` and `CreatureMindSystemTest.SwappedMindsCrossOverWhileEachBodyKeepsItsNameSizeAndAlignment`; testbed `mind.creature_swap`. This row said the name moved with the mind: runblack.exe v1.42's `Creature::SwapMinds` (0x47BEE0) copies both names out at 0x47BF49..0x47BF6C, loads each mind into the other body (0x47BFE4..0x47C1AB) and copies each body's own name back at 0x47C1B0..0x47C1D3, then puts back each body's own size (physical +0x6C) and alignment (`CrudeSet`): see [Swapping minds](../../../../creature/players-creature.md#swapping-minds); finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Swaps the player's creature for another species", [saves_and_files.md](../../creature/saves_and_files.md) "A mind can be loaded into an existing creature"; still open: no quest run's PASS is cited for it |
| Ownership swaps: the new body becomes the player's creature and the old body belongs to no one | partial 60% | no swap run in play yet (no quest_check PASS), so the code and tests below are the measured share; ported from raffclar PR #384, unverified against v1.42: `SwapMinds` gives each body the other's owner (`LeashSystemInterface::SetOwner`, `ClaimOnArrival`, `PlayerSystemInterface::AddCreature`); the leashes off first, and the leashes the player taught go with the creature they lead; tests `CreatureMindSystemTest.SwappedMindsCrossOverWhileEachBodyKeepsItsNameSizeAndAlignment`, `LeashSystemTest.TheOneLedIsKeptAsThePlayersCreature`; finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Swaps the player's creature for another species", [decision_making.md](../../creature/decision_making.md) "A script gives a creature to another player"; still open: no quest run's PASS is cited for it |
| Both creatures then play a "mind swapped with another creature" action | partial 50% | no swap run in play yet (no quest_check PASS), so the code and tests below are the measured share; ported from raffclar PR #384, unverified against v1.42: both are forced to carry out the table's `SwapMindWithOtherCreature` (`ForceActivity`), whose step `SwapMind` waits for the spells on both to end, plays action 109 and starts the swap beam (spot visual 41, 70 turns) in the owner's colour; the beam's far end on the other creature is not set yet; finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Swaps the player's creature for another species", [creature_casting.md](../../creature/creature_casting.md) "It swaps minds with another creature"; still open: no quest run's PASS is cited for it |
| The new body's miracle-learning thresholds are recalculated for its species | partial 60% | no swap run in play yet (no quest_check PASS), so the code and tests below are the measured share; ported from raffclar PR #384, unverified against v1.42: the sightings of each miracle the mind moving into the new body has are scaled by the times its new species must see it over its old one's (`creature_swap::RescaledSightings`); test `CreatureSwap.SightingsKeepTheirShareInTheNewSpecies`; finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Swaps the player's creature for another species"; still open: no quest run's PASS is cited for it |
| Afterwards the new creature can be picked up again and the old one can't; the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | pending | no swap checked in play (`SWAP_CREATURE` is real) |
| The old body stays where it is, idle, and the offer starts again over it, so the player can swap back and forth as often as they like | pending | no swap checked in play (`SWAP_CREATURE` is real) |
| The story's record of the current species is updated, which the breeder and later quests read | pending | `GAME_SUB_TYPE` and `SWAP_CREATURE` are real |

## The breeder's offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each offered creature gets its own scroll and its own random 60 to 120 second timer; when any timer runs out the whole offer closes | pending | `CREATE` makes the breeder's creatures now; not checked in play; `SWAP_CREATURE` is ported, unverified against v1.42 |
| Only one creature may be chosen at a time: clicking a second one while another is chosen cancels both and the chosen one walks back | pending | `CREATE` makes the breeder's creatures now; not checked in play; `SWAP_CREATURE` is ported, unverified against v1.42 |
| On confirmation both creatures walk to the land's two swap spots (up to 20 seconds) before the same swap cut scene | pending | `CREATE` makes the breeder's creatures now; not checked in play; `SWAP_CREATURE` is ported, unverified against v1.42 |
| After the swap the old body walks back to where the new creature stood, idles, and the breeder's offer closes | pending | `CREATE` makes the breeder's creatures now; not checked in play; `SWAP_CREATURE` is ported, unverified against v1.42 |
| When the offer closes, every creature still on offer (including the player's old body) vanishes with a sparkle and a puff of smoke | pending | `CREATE` makes the breeder's creatures now; not checked in play; `SWAP_CREATURE` is ported, unverified against v1.42 |

## Cut swap scrolls (never in the game)

These five scripts are in the source but not in the compiled challenge file, and nothing in the shipped game starts
them. Their titles exist in the text table ("Swap To Horse", "Swap To Leopard", "Stanley The Wolf" for the lion, "Swap To
Turtle", "Swap To Wolf") but none of their dialogue lines do, and only the wolf's reminder line exists ("This poor
shepherd keeps losing his sheep."). The horse and tortoise scripts say "no longer used" at the top. The species they
offered went elsewhere: horse and leopard to the breeder ([the_creature_breeder.md](./the_creature_breeder.md)), lion to
"Stanley The Wolf" ([stanley_the_wolf.md](./stanley_the_wolf.md)), tortoise to "The Fish Puzzle"
([the_fish_puzzle.md](./the_fish_puzzle.md)), wolf to the blind woman ([the_treacherous_path.md](./the_treacherous_path.md)).
