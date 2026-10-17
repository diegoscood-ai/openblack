# Creature swaps

The shared offer that ends every quest whose reward is a new creature: a creature appears under a silver scroll, the
player brings their own creature to it and clicks twice, and the creature's mind moves into the new body. A second,
stricter version runs the creature breeder's kennels. This file also covers the five swap scrolls that were written but
cut before release (horse, leopard, lion, tortoise, wolf).

**Land:** all (run by quests on lands 1 to 5) · **Giver:** the quest that ends in a creature · **Script:** SwapCreatures, SwapCreaturesAtBreeder · **Reward:** the new species for the player's creature · **Repeatable:** yes (the field offer never ends)

**Progress: 0/38 done, 0 partial — 0%**

Sources: the swap script and every script that runs it (the original source text, matching the PC game's compiled
`challenge.chl`), the game's text table and the executable's swap command. openblack's state is judged on this tree:
`SwapCreature` and `IsCreatureAvailable` are stubs in `src/CHLApi.cpp`, and so are the creature-control commands the
offer needs (its dialogue, highlight and special-effect commands work). openblack can build a creature of any species
with a mind (`CreatureArchetype::Create`, used by the dev spawner) but has nothing that moves a mind into another body,
so every row is todo unless it says otherwise.

## Who offers a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1, "The Lost Flock": a sheep creature, once the whole flock is penned | todo | The Lost Flock starts with the fourth tutorial answer, but `CREATE` does not make creatures, so no sheep creature is ever offered |
| Land 2, "The Riddles": a zebra appears on the altar | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 3, "The Rejuvenator": an ape (a chimp when the player's creature is already an ape) | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 4, "The Treacherous Path" (the blind woman): a wolf | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 4, "The Fish Puzzle": a tortoise | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 5, "Stanley The Wolf": a lion (the tamer can't die, so it is always offered) | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 5, "The Explorers Again": a polar bear | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| Land 5, "Swap To Brown Bear": a brown bear | todo | no Land 2 to 5 in our tree (`LOAD_MAP` is empty), and `CREATE` does not make creatures |
| The creature breeder on lands 1 and 4: the five special creatures and the player's previous creature | todo | the breeder runs on Land 1 (fourth answer) but shows nothing: `IS_CREATURE_AVAILABLE` is a stub that answers no |
| Never offered in the shipped game: a cow ("Swap To Cow" is compiled but never started) and a chimp ("The Miracle Stones" is not compiled) | n/a | never started by the game: [swap_to_cow.md](./swap_to_cow.md), [the_miracle_stones.md](./the_miracle_stones.md) |

## The offer in the field

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The offered creature can't be picked up while it is on offer | todo | no creature is ever offered (`CREATE` does not make creatures); `SET_ID_PICKUPABLE` and `CREATE_HIGHLIGHT` themselves work |
| A silver scroll appears over the offered creature, one unit above its height | todo | no creature is ever offered (`CREATE` does not make creatures); `SET_ID_PICKUPABLE` and `CREATE_HIGHLIGHT` themselves work |
| Until clicked, the creature keeps turning to face the camera and plays its look-at-camera action | todo | no creature is ever offered; `SET_FOCUS` and the clips on a creature are not ported either |
| While the camera is within 100 of the scroll and the creature is on screen, the evil advisor steps out at most once a minute, points at the scroll and says "Swap your Creature with this one if you want." | todo | no creature is ever offered; the notice script, `GAME_THING_CLICKED` and the advisors work |
| Clicking the scroll or the creature itself starts the swap | todo | no creature is ever offered; the notice script, `GAME_THING_CLICKED` and the advisors work |
| If the player's creature is not within 50 of it, the evil advisor says "You'll need to bring our Creature, Boss." and the scroll comes back for another click | todo | no creature is ever offered; the notice script, `GAME_THING_CLICKED` and the advisors work |
| Otherwise the scroll goes, the leash is taken off and the player's creature is walked to within 15 of the new one; the game waits up to 5 seconds for it to get within 20 | todo | no creature is ever offered; moving or turning a creature from a script (`MOVE_GAME_THING`, `SET_FOCUS` on a creature) is not ported |
| Both creatures turn and examine each other by looking | todo | no creature is ever offered; moving or turning a creature from a script (`MOVE_GAME_THING`, `SET_FOCUS` on a creature) is not ported |
| The good advisor asks, every 30 seconds until answered: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel." (the last token is replaced by the action-button picture) | todo | no creature is ever offered; the timer and `RUN_TEXT` work |
| Clicking the new creature confirms; clicking the player's own creature cancels and frees it; after 110 seconds with no answer the offer cancels itself | todo | no creature is ever offered; the timer and `RUN_TEXT` work |
| A cancelled offer puts the scroll back over the new creature and starts again from the first click | todo | no creature is ever offered; the timer and `RUN_TEXT` work |
| A random 60 to 120 second offer timer is made but never checked in the field offer, so the offer never runs out | todo | no creature is ever offered; the timer and `RUN_TEXT` work |

## The swap itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A short cut scene with a two-subject camera framing both creatures, which face each other | todo | no creature is ever offered; `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work |
| After a second, a spell-success sparkle appears on each creature, the leash comes off again and they point at each other | todo | no creature is ever offered; `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work |
| The swap sound plays as the bodies are swapped, and the cut scene waits until both have finished their animations, then 3 more seconds | todo | no creature is ever offered; `START_DUAL_CAMERA`, `SPECIAL_EFFECT_OBJECT` and `PLAY_SOUND_EFFECT` work |
| The new body takes on the player's creature's mind: everything it has learnt, its name, its alignment and its built-up size and strength values move with it; both bodies are re-shaped from those values | todo | `SWAP_CREATURE` is a stub; our tree builds a creature of any species with a mind (`CreatureArchetype::Create`) but nothing moves a mind into another body |
| Ownership swaps: the new body becomes the player's creature and the old body belongs to no one | todo | `SWAP_CREATURE` is a stub; our tree builds a creature of any species with a mind (`CreatureArchetype::Create`) but nothing moves a mind into another body |
| Both creatures then play a "mind swapped with another creature" action | todo | `SWAP_CREATURE` is a stub; our tree builds a creature of any species with a mind (`CreatureArchetype::Create`) but nothing moves a mind into another body |
| The new body's miracle-learning thresholds are recalculated for its species | todo | `SWAP_CREATURE` is a stub; our tree builds a creature of any species with a mind (`CreatureArchetype::Create`) but nothing moves a mind into another body |
| Afterwards the new creature can be picked up again and the old one can't; the evil advisor says "Great, Boss. If you want your old Creature back, just return here later." | todo | no swap (`SWAP_CREATURE` is a stub) |
| The old body stays where it is, idle, and the offer starts again over it, so the player can swap back and forth as often as they like | todo | no swap (`SWAP_CREATURE` is a stub) |
| The story's record of the current species is updated, which the breeder and later quests read | todo | `GAME_SUB_TYPE` and `SWAP_CREATURE` are stubs |

## The breeder's offer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each offered creature gets its own scroll and its own random 60 to 120 second timer; when any timer runs out the whole offer closes | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |
| Only one creature may be chosen at a time: clicking a second one while another is chosen cancels both and the chosen one walks back | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |
| On confirmation both creatures walk to the land's two swap spots (up to 20 seconds) before the same swap cut scene | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |
| After the swap the old body walks back to where the new creature stood, idles, and the breeder's offer closes | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |
| When the offer closes, every creature still on offer (including the player's old body) vanishes with a sparkle and a puff of smoke | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |
| The breeder records both the new and the old species, so the old one can be swapped back later | todo | the breeder never shows a creature (`IS_CREATURE_AVAILABLE` is a stub) and `SWAP_CREATURE` is a stub |

## Cut swap scrolls (never in the game)

These five scripts are in the source but not in the compiled challenge file, and nothing in the shipped game starts
them. Their titles exist in the text table ("Swap To Horse", "Swap To Leopard", "Stanley The Wolf" for the lion, "Swap To
Turtle", "Swap To Wolf") but none of their dialogue lines do, and only the wolf's reminder line exists ("This poor
shepherd keeps losing his sheep."). The horse and tortoise scripts say "no longer used" at the top. The species they
offered went elsewhere: horse and leopard to the breeder ([the_creature_breeder.md](./the_creature_breeder.md)), lion to
"Stanley The Wolf" ([stanley_the_wolf.md](./stanley_the_wolf.md)), tortoise to "The Fish Puzzle"
([the_fish_puzzle.md](./the_fish_puzzle.md)), wolf to the blind woman ([the_treacherous_path.md](./the_treacherous_path.md)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Is it in the game: none of the five is compiled, started, or reachable; the text table has their titles only | n/a | never started by the game: checked against every land's control and challenge list |
| Swap To Horse: out of view, a horse creature and an Indian trader appear at a fixed spot; a silver scroll and the evil advisor's "Hey. There's a job for you. Wanna do it?"; on click, a cut scene where the trader walks up and speaks four lines, the horse steps forward, both advisors come out for five more lines; then the field offer | n/a | never started by the game: all nine lines are missing from the text table; its challenge record lines were commented out because they "don't compile"; the spot is not near any shipped land's town |
| Swap To Leopard: the challenge record opens, a leopard creature appears out of view under a silver scroll ("There's a Silver Reward Scroll down here, Boss."), then a cut scene of twelve advisor lines and the field offer; the record closes at once as a success | n/a | never started by the game: all twelve lines missing; the leopard became a breeder creature |
| Swap To Lion: a tamer and a wild lion (an animal, not a creature) by a flock; the good advisor's "Your attention is required here."; the lion hunts nearby cattle; if the player carries it away from the tamer and puts it down alive, it turns into a lion creature after a cut scene and is offered (record closes good); if it dies, the advisors comment and the record closes evil | n/a | never started by the game: all sixteen lines missing; a debug line "Picked up Lion" would have shown while it was held; meant to be placed outside the player's influence |
| Swap To Turtle: out of view, a Japanese farmer and a flock of five tortoises at 70% health; heal a tortoise to full health and the farmer gives a tortoise creature after a cut scene | n/a | never started by the game: all lines missing; its spot is about 80 from Land 4's Japanese town, the land "The Fish Puzzle" later gave a tortoise on |
| Swap To Wolf: a shepherd and a flock of twenty sheep, one of which is secretly a wolf; picking that sheep up turns it into a wolf creature in the hand, with an extra line if it was the first thing picked up; then the field offer | n/a | never started by the game: all lines missing (an earlier copy of the same script exists as a test file that starts it at the hand's position) |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The field offer never ends, so a quest's offered creature keeps its silver scroll for the rest of the land even after the player has swapped | todo | no creature is ever offered |
| When "Swap To Cow" (unused) gave nothing because the creature was already a cow, it still ran the offer with no creature | n/a | never started by the game: see [swap_to_cow.md](./swap_to_cow.md) |
