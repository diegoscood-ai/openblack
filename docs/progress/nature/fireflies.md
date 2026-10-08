# Fireflies

Small glowing lights that leave the trees and rocks at nightfall, hover around the houses and street lights all night
and hide again in the nearest tree or rock at dawn. By day they can't be seen; lifting a tree or rock with the hand while
a firefly hides in it removes the firefly and leaves a one-shot miracle seed where it stood, drawn from the land's
firefly reward table. This is the "miracles hidden under trees and rocks" players remember: no seed is ever placed under
a tree; it comes from a hiding firefly (see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md)).

The game hints at it three ways: the Hermit on Land 1 saw "a firefly a-heading under that there rock at break of dawn"
and the seed under his rock is a scripted version of it ([../story/silver_scrolls/the_hermit.md](../story/silver_scrolls/the_hermit.md));
a Land 2 "did you know" scroll; and a tip of the day (rows below). The game keeps no statistic of fireflies caught, and
neither the creature nor the villagers take any notice of them.

**Progress: 39/43 done, 1 partial — 92%**

## What a firefly is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is a single small white glowing sprite, 0.3 m in size, from the game's third sprite sheet; it has no model of its own | done | `src/ECS/FireFlies.cpp` (frame 37 of `S_SpriteSheet3`, half size 0.3, additive) |
| It makes no sound, has no light of its own on the land, and takes no part in physics: nothing can hit it or push it | done | `src/ECS/FireFlies.cpp`: a sprite only |
| The game's object table lists it as a nice animal named "Firefly", with a bat model that is never drawn | done | Our tree draws only the sprite; the info row is in `src/Enums.h` |
| It is drawn only while out of its hiding place, and not at all beyond 300 m from the camera | done | `ecs::UpdateFireFlies` (nothing asleep or past 300 m) |
| Within 100 m it is drawn at about three-quarters opacity (190 of 255); further away it fades, by the square of the distance, to nothing at 300 m | done | `ecs::UpdateFireFlies` (alpha 190, fading by the squared distance to 300 m) |
| Its drawn place is blended between the last two game turns' places, so it moves smoothly between turns | done | `ecs::UpdateFireFlies` (the turn fraction) |

## Where they come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land holds at most 50 fireflies | done | `k_MaxFireFlies` in `src/ECS/FireFlies.cpp` |
| At the first nightfall after a land is loaded, and at each nightfall that follows a morning, the game makes new fireflies to bring the count back up to 50 | done | `ecs::ProcessFireFliesTurn` (the top-up, once a night, allowed again each morning), from `src/Game.cpp` |
| Each new firefly is hidden, by a coin toss, either in a random tree anywhere on the land or in a rock: a random place in the land's list of fixed objects, then the first rock from there (none from there makes nothing this time) | done | `Spawn` in `src/ECS/FireFlies.cpp` (a random tree, or a random multi-map fixed object and then the first rock) |
| Each firefly draws its own speed, between 0.6 and 1.4 times the normal, for its flights and its drifting, and random starting points for its drift | done | `Create` in `src/ECS/FireFlies.cpp` (eight synced draws in the game's order) |
| The land scripts can also place fireflies at a spot; the story lands place none and rely on the top-up | todo | `FeatureScriptCommands::CreateFireFly` is an empty stub; see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |
| Three Gods places 43, the demo map 38 and Death Comes To Those That Wait 86 (two lists of 43); most stand exactly on one of the map's trees (35 of 43, 26 of 38, but only 35 of 86 on Death Comes, whose lists partly repeat Three Gods' spots) | todo | CREATE_FIRE_FLY is not ported; see [../multiplayer/maps/](../multiplayer/maps/) |
| At nightfall a firefly whose spot no longer holds a tree or rock, or that shares its spot with another firefly, is removed instead of coming out | done | `WakeOne` in `src/ECS/FireFlies.cpp` |

## Night and day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the evening, once the sky leaves dusk and starts to darken towards night, the fireflies come out, one each game turn | done | `ecs::ProcessFireFliesTurn` (one a turn, by the sky type of the shown clock); see [../sky/day_night_cycle.md](../sky/day_night_cycle.md) |
| In the morning, once the sky leaves dawn and starts to brighten towards day, they go back into hiding, one each game turn | done | `ecs::ProcessFireFliesTurn` (`SleepOne`) |
| Between those times (late morning to dusk, and the dark hours until dawn) nothing changes: hidden ones stay hidden and those out stay out | done | `ecs::ProcessFireFliesTurn` |
| Only the shown time of day matters: weather, rain, season, the land's alignment and the date make no difference | done | `ecs::ProcessFireFliesTurn` reads only the day and night clock |

## Flying and hovering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Coming out, a firefly flies to a house or street light within 300 m of its hiding place, roughly the nearest (it searches a random half of the ground squares around it and takes the nearest it finds), and hovers 2 m above its top | done | `WakeOne` and the spiral search in `src/ECS/FireFlies.cpp` (each cell on a coin toss, its height plus 2) |
| With no house or street light in reach, it hovers 15 m to one side of its hiding place, 4 m up | done | `WakeOne` (15 m along x, altitude 4) |
| A flight takes its length over 3 m a second times the firefly's own speed, never less than half a second, and eases in and out | done | `StartFlight` (at least 0.5 s), eased in `ecs::ProcessFireFliesTurn` |
| Hovering, it drifts around its spot on a slow loop of 8 m (4 m up and down), turning 0.06 to 0.14 radians a second by its speed, with a quick loop of 1 m (half a metre up and down) on top turning 0.6 to 1.7 radians a second | done | `ecs::UpdateFireFlies` (the two loops) |
| The drift grows over the first fifth of the flight out and dies away over the last fifth of the flight home, so it rests still in its hiding place | done | `ecs::UpdateFireFlies` |
| Going home, it hides in a tree or rock within 300 m of where it hovered, roughly the nearest; with none it settles on the ground 15 m to the side (and is removed at nightfall) | done | `SleepOne` (the same search; none: 15 m along x on the ground) |
| So fireflies gather in the trees and rocks next to houses and street lights; a tip of the day says "If you place rocks and trees next to houses they will attract fireflies more readily." | done | Follows from the searches; whether the tip of the day is shown is not confirmed |
| They follow nothing: not the hand, the creature, villagers or the camera | done | `src/ECS/FireFlies.cpp` |

## Catching one

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is never picked up or tapped itself; it is caught when the player's hand picks up the tree or rock it is hiding in, uprooting the tree or lifting the rock | done | `fire_fly::OnPlacedInMagicHand` (`src/Worship/FireFlyReward.cpp`) from the hand's pick-up (`HandHolding.cpp`); see [../hand/picking_up.md](../hand/picking_up.md) |
| It must sit exactly at that tree or rock's spot, so only one at rest counts: in practice from dawn until it is sent out after dusk; one already out hovering by a house is not caught | done | `ecs::TakeFireFlyAt` (the same x and z) |
| The firefly is removed and a one-shot miracle seed appears where the tree or rock stood, at full strength and at the power-up level of the miracle drawn | done | `fire_fly::Reward` (`magic::one_off::Create` at the spot, strength 1, the magic's power-up level); see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md) |
| One pick-up catches at most one firefly | done | `ecs::TakeFireFlyAt` |
| The miracle is drawn by weight: each kind's chance is its weight over the sum of all weights; with every weight at zero the firefly is lost and nothing appears | done | `fire_fly::Reward` (a synced draw over the running sums; nothing when they are 0) |
| Only the god's hand catches them: the creature lifting a tree or rock, foresters felling, the tug shaking a tree, fire, or a tree knocked down never give a seed; the firefly that lost its home is removed at nightfall | done | Only the hand's pick-up calls `fire_fly::OnPlacedInMagicHand` |
| Any player's hand can catch them in a multiplayer game, and the fireflies' choices use the game's shared random numbers so every machine agrees | partial | The fireflies use the synced game random numbers; our tree has no multiplayer. See [../multiplayer/](../multiplayer/) |

## The land's reward table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every land starts with all weights at zero, then its script sets them one miracle at a time; a weight for a kind the game doesn't have is ignored | done | `fire_fly::Reset` and `fire_fly::SetRewardProbability` (FIRE_FLY_SPELL_REWARD_PROB, `src/LHScriptX/FeatureScriptCommands.cpp`). Our wiki differs: a new land clears only the weights; the running sums the draw reads keep the last land's values until the script sets one ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)) |
| Land 1: heal 20 of 26 (77%); fireball, lightning, forest, food, wood and water 1 each (about 4%) | done | Read from Land 1's script; [../scripts/land1_script.md](../scripts/land1_script.md) |
| Land 2: fireball, lightning, heal, teleport, forest, food, storm, spiritual shield, physical shield, wood and water about 8% each; flying flock, and the creature's freeze, small, big, weak, strong, invisible, compassion, angry and itchy under 1% each; ground flock about 0.1% | done | Read from the land's script. Our wiki differs: only Land 1's script sets firefly rewards ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)); [../scripts/land2_script.md](../scripts/land2_script.md) |
| Land 3: the same eleven at about 9% each and both flocks under 1%; no creature miracles | done | Read from the land's script. Our wiki differs: only Land 1's script sets firefly rewards ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)); [../scripts/land3_script.md](../scripts/land3_script.md) |
| Land 4: the eleven, with stronger heal and stronger food, about 7% each; stronger fireball, lightning and water about 1.4%; strongest fireball and lightning and the storm with lightning about 0.7%; the nine creature miracles about 0.7% each; no flocks | done | Read from the land's script. Our wiki differs: only Land 1's script sets firefly rewards ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)); [../scripts/land4_script.md](../scripts/land4_script.md) |
| Land 5: the eleven and stronger food about 6% each; both flocks about 3%; strongest fireball, stronger lightning, stronger heal and the nine creature miracles about 1.3%; stronger fireball, strongest lightning, the explosion miracle, the storm with lightning and the tornado about 0.6% | done | Read from the land's script. Our wiki differs: only Land 1's script sets firefly rewards ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)); [../scripts/land5_script.md](../scripts/land5_script.md); see [../miracles/blast.md](../miracles/blast.md) |
| The tutorial land's weights are all zero, so a firefly caught there gives nothing | done | Read from the land's script; [../scripts/landT_script.md](../scripts/landT_script.md) |
| No table ever names the fat, thin, hungry, frightened, tired, ill or thirsty creature miracles, so fireflies never give them | done | Data-driven; see [../miracles/creature_spells.md](../miracles/creature_spells.md) |
| Skirmish: Two Gods and Four Gods share one table (fireball, lightning, heal, forest, food, storm, both shields, wood and water about 8% each, both flocks about 4%, teleport and the nine creature miracles about 1.5%, the explosion under 1%); Death Comes To Those That Wait ends with Land 2's table; Three Gods, Island Wars and Firestorm set every weight to zero, so their fireflies give nothing | done | Read from each map's script; see [../multiplayer/maps/](../multiplayer/maps/) |

## Saving and editing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps every firefly (its spot, whether it is out, hiding or flying, and its flight), the reward table, the limit of 50 and whether the next nightfall tops them up | todo | openblack has no saved games |
| The land editor writes each firefly into the land's script as a placement at its spot | n/a | openblack has no land editor |

## Hints in the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Land 2 "did you know" scroll: "Fireflies can be seen at night. They hide under rocks during the day, and become rewards if you find them." | done | The did-you-know scrolls are read when tapped (`src/Help/Bubble.cpp`); see [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
