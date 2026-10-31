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

**Progress: 36/43 done, 0 chosen, 4 partial, 0 inferred — 91%**

## What a firefly is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is a single small glowing sprite, 0.3 m in size, from the game's third sprite sheet; it has no model of its own | done | `src/ECS/FireFlies.cpp` (frame 37 of `S_SpriteSheet3`, half size 0.3, additive); scene `sky.fireflies_evening`; test `FireFliesWorld.AFireflyIsOneSmallGlowingSpriteOfTheThirdSheet` (`test/test_fire_flies_world.cpp`, on synthetic and the game's data) |
| It makes no sound, has no light of its own on the land, and takes no part in physics: nothing can hit it or push it | done | `src/ECS/FireFlies.cpp`: a sprite only; scene `sky.fireflies_evening`; test `FireFliesWorld.AFireflyHasNoModelAndNoPartInThePhysics` (`test/test_fire_flies_world.cpp`, on synthetic and the game's data) |
| The game's object table lists it as a nice animal named "Firefly", with a bat model that is never drawn | done | Our tree draws only the sprite (test `FireFliesWorld.AFireflyHasNoModelAndNoPartInThePhysics`); the type is in `src/Enums.h`, a table entry with no logic to test |
| It is drawn only while out of its hiding place, and not at all beyond 300 m from the camera | done | `ecs::UpdateFireFlies` (nothing asleep or past 300 m); scene `sky.fireflies_evening`; test `FireFlies.OneOutIsDrawnAt190WithinAHundredMetresFadingToNoneAtThreeHundred` |
| Within 100 m it is drawn at about three-quarters opacity (190 of 255); further away it fades, by the square of the distance, to nothing at 300 m | done | `ecs::UpdateFireFlies` (alpha 190, fading by the squared distance to 300 m); scene `sky.fireflies_evening`; the alpha truncated to a whole step as the original's byte (`fire_flies::Alpha`); test `FireFlies.OneOutIsDrawnAt190WithinAHundredMetresFadingToNoneAtThreeHundred` |
| Its drawn place is blended between the last two game turns' places, so it moves smoothly between turns | done | `ecs::UpdateFireFlies` (`game_clock::TurnFraction`, `src/ECS/FireFlies.cpp`); scene `sky.fireflies_evening`; no unit test: a draw-time blend checked by the scene's shots |

## Where they come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land holds at most 50 fireflies | done | `k_MaxFireFlies` in `src/ECS/FireFlies.cpp`; test `test/test_fire_flies.cpp`, `FireFlies.ALandHoldsAtMostFifty` |
| At the first nightfall after a land is loaded, and at each nightfall that follows a morning, the game makes new fireflies to bring the count back up to 50 | done | `ecs::ProcessFireFliesTurn` (the top-up, once a night, allowed again each morning), from `src/Game.cpp`; scene `sky.fireflies_evening`; test `FireFlies.ALandHoldsAtMostFifty` (the first nightfall's top-up to 50, none the next night while 50 are about) |
| Each new firefly is hidden, by a coin toss, either in a random tree anywhere on the land or in a rock: a random place in the land's list of fixed objects, then the first rock from there, a bonfire or a piece knocked off a building counting as a rock (none from there makes nothing this time) | done | `Spawn` in `src/ECS/FireFlies.cpp` (a random tree, or a random multi-map fixed object and then the first rock, `ecs::FireFlyRockFrom`); test `FireFliesTest.AFireFlyNotAtATreeStartsAtTheFirstRockOfTheFixedListFromItsDraw`; scene `sky.fireflies_evening` |
| Each firefly draws its own speed, between 0.6 and 1.4 times the normal, for its flights and its drifting, and random starting points for its drift | done | `Create` in `src/ECS/FireFlies.cpp` (eight synced draws in the game's order); scene `sky.fireflies_evening`; no unit test of the draws: their order is checked by the fidelity runs' random trace |
| The land scripts can also place fireflies at a spot; the story lands place none and rely on the top-up | pending | `FeatureScriptCommands::CreateFireFly` is an empty stub; see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |
| Three Gods places 43 and the demo map 38; most stand exactly on one of the map's trees (35 of 43, 26 of 38) | pending | CREATE_FIRE_FLY is not ported; see [../multiplayer/maps/](../multiplayer/maps/) |
| At nightfall a firefly whose spot no longer holds a tree or rock, or that shares its spot with another firefly, is removed instead of coming out | partial 90% | `WakeOne` in `src/ECS/FireFlies.cpp`; a bonfire and a fragment are rocks here (`ecs::IsFireFlyPerch`, test `FireFliesTest.AFireFlyPerchesOnARockTheBonfireAFragmentOrAnyTree`); the spot's map coordinates compared on x, z and the altitude, as the original; the removal where the tree has gone, test `FireFlies.WithNoTreeOrRockToGoHomeToItRests15MetresAsideOnTheGroundAndGoesAtNightfall`; missing: no test of a spot shared with another firefly, nor of the altitude compare |

## Night and day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the evening, once the sky leaves dusk and starts to darken towards night, the fireflies come out, one each game turn | done | `ecs::ProcessFireFliesTurn` (one a turn, by the sky type of the shown clock); see [../sky/day_night_cycle.md](../sky/day_night_cycle.md); scene `sky.fireflies_evening`; tests `FireFlies.BetweenMorningAndDuskAndBetweenNightAndDawnNothingChanges` (none before dusk, out after it), `FireFlies.WithNoTreeOrRockToGoHomeToItRests15MetresAsideOnTheGroundAndGoesAtNightfall` (one each evening turn) |
| In the morning, once the sky leaves dawn and starts to brighten towards day, they go back into hiding, one each game turn | done | `ecs::ProcessFireFliesTurn` (`SleepOne`); tests `FireFlies.GoingHomeItHidesInTheNearestTreeOrRock` (the morning's return), `FireFlies.InTheMorningTheyGoHomeOneEachTurn` (the pace) |
| Between those times (late morning to dusk, and the dark hours until dawn) nothing changes: hidden ones stay hidden and those out stay out | done | `ecs::ProcessFireFliesTurn`; test `FireFlies.BetweenMorningAndDuskAndBetweenNightAndDawnNothingChanges` |
| Only the shown time of day matters: weather, rain, season, the land's alignment and the date make no difference | done | `ecs::ProcessFireFliesTurn` reads only the day and night clock; the tests of the rows above drive it by the clock alone (`FireFlies.BetweenMorningAndDuskAndBetweenNightAndDawnNothingChanges`) |

## Flying and hovering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Coming out, a firefly flies to a house or street light within 300 m of its hiding place, roughly the nearest (it searches a random half of the ground squares around it and takes the nearest it finds), and hovers 2 m above its top | done | `WakeOne` and the spiral search in `src/ECS/FireFlies.cpp` (each cell on a coin toss, its height plus 2; none: 15 m along x at 4); tests `test/test_fire_flies.cpp`, `FireFlies.ComingOutItHoversTwoMetresAboveTheNearestLightItFinds` and `FireFlies.WithNoLightWithinThreeHundredMetresItHovers15MetresAsideAtFour` (street lanterns; no scene with houses) |
| With no house or street light in reach, it hovers 15 m to one side of its hiding place, 4 m up | done | `WakeOne` (15 m along x, altitude 4); scene `sky.fireflies_evening`; test `FireFlies.WithNoLightWithinThreeHundredMetresItHovers15MetresAsideAtFour` |
| A flight takes its length over 3 m a second times the firefly's own speed, never less than half a second, and eases in and out | done | `StartFlight` (at least 0.5 s), eased in `ecs::ProcessFireFliesTurn`; scene `sky.fireflies_evening`; test `FireFlies.AFlightTakesItsLengthOverThreeMetresASecondTimesItsSpeedAtLeastHalfASecond` |
| Hovering, it drifts around its spot on a slow loop of 8 m (4 m up and down), turning 0.06 to 0.14 radians a second by its speed, with a quick loop of 1 m (half a metre up and down) on top turning 0.6 to 1.7 radians a second | done | `ecs::UpdateFireFlies` (the two loops); scene `sky.fireflies_evening`; test `FireFlies.ComingOutItHoversTwoMetresAboveTheNearestLightItFinds` (the 8 m and 1 m loops at their starting phases) |
| The drift grows over the first fifth of the flight out and dies away over the last fifth of the flight home, so it rests still in its hiding place | done | `fire_flies::DriftAmplitude` (`src/ECS/FireFlies.cpp`), the flight home's factor the original's float a hair over 5; tests `FireFlies.TheDriftGrowsOverTheFirstFifthOut`, `FireFlies.TheDriftDiesOverTheLastFifthHomeByTheOriginalsFactor` (`test/test_fire_fly_drift.cpp`) ([day-and-night.md](../../../world/day-and-night.md#night-lights)) |
| Going home, it hides in a tree or rock within 300 m of where it hovered, roughly the nearest; with none it settles on the ground 15 m to the side (and is removed at nightfall) | done | `SleepOne` (the same search; none: 15 m along x on the ground); tests `test/test_fire_flies.cpp`, `FireFlies.GoingHomeItHidesInTheNearestTreeOrRock` and `FireFlies.WithNoTreeOrRockToGoHomeToItRests15MetresAsideOnTheGroundAndGoesAtNightfall` (no scene) |
| So fireflies gather in the trees and rocks next to houses and street lights; a tip of the day says "If you place rocks and trees next to houses they will attract fireflies more readily." | done | Follows from the searches; whether the tip of the day is shown is not confirmed; tests `FireFlies.ComingOutItHoversTwoMetresAboveTheNearestLightItFinds`, `FireFlies.GoingHomeItHidesInTheNearestTreeOrRock` (from the light, the tree nearest it) |
| They follow nothing: not the hand, the creature, villagers or the camera | done | `src/ECS/FireFlies.cpp`; test `FireFliesWorld.AFireflyFollowsNothingNotEvenTheCamera` (`test/test_fire_flies_world.cpp`, on synthetic and the game's data) |

## Catching one

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A firefly is never picked up or tapped itself; it is caught when the player's hand picks up the tree or rock it is hiding in, uprooting the tree or lifting the rock | done | `fire_fly::OnPlacedInMagicHand` (`src/Worship/FireFlyReward.cpp`) from the hand's pick-up (`HandHolding.cpp`); see [../hand/picking_up.md](../hand/picking_up.md); test `test/test_fire_flies.cpp`, `FireFlies.PickingUpTheTreeItHidesInCatchesIt` |
| The firefly is removed and a one-shot miracle seed appears where the tree or rock stood, at full strength and at the power-up level of the miracle drawn | done | `fire_fly::Reward` (`magic::one_off::Create` at the spot, strength 1, the magic's power-up level); see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md); test `test/test_worship.cpp`, `WorshipTest.AFireflyRewardIsAOneShotAtFullStrength` |
| One pick-up catches at most one firefly | done | `ecs::TakeFireFlyAt`; test `test/test_fire_flies.cpp`, `FireFlies.OnePickUpCatchesOneFirefly` |
| It must sit exactly at that tree or rock's spot, so only one at rest counts: in practice from dawn until it is sent out after dusk; one already out hovering by a house is not caught | done | `ecs::TakeFireFlyAt` (the object's map coordinates, x, z and the altitude, as the original); tests `FireFlies.PickingUpTheTreeItHidesInCatchesIt` (the tree they left is empty), `FireFlies.OnePickUpCatchesOneFirefly`, `FireFlies.OnesOutHoveringAreNotCaught`, `FireFlies.TheTreeMustBeAtTheirSpotsAltitudeToo` |
| The miracle is drawn by weight: each kind's chance is its weight over the sum of all weights; with every weight at zero the firefly is lost and nothing appears | done | `fire_fly::Reward` (a synced draw over the running sums; nothing when they are 0); test `WorshipTest.FireFlyRewardProbabilities` |
| Only the god's hand catches them: the creature lifting a tree or rock, foresters felling, the tug shaking a tree, fire, or a tree knocked down never give a seed; the firefly that lost its home is removed at nightfall | partial 90% | Only the hand's pick-up calls `fire_fly::OnPlacedInMagicHand`; no test |
| Any player's hand can catch them in a multiplayer game, and the fireflies' choices use the game's shared random numbers so every machine agrees | partial 50% | The fireflies use the synced game random numbers; our tree has no multiplayer. See [../multiplayer/](../multiplayer/); missing: multiplayer |

## The land's reward table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every land starts with all weights at zero, then its script sets them one miracle at a time; a weight for a kind the game doesn't have is ignored | done | `fire_fly::Reset` and `fire_fly::SetRewardProbability` (FIRE_FLY_SPELL_REWARD_PROB, `src/LHScriptX/FeatureScriptCommands.cpp`). Our wiki differs: a new land clears only the weights; the running sums the draw reads keep the last land's values until the script sets one ([magic.md](../../../magic/worship.md#dispensers-and-fireflies)); test `WorshipTest.FireFlyRewardProbabilities` |
| Land 1: heal 20 of 26 (77%); fireball, lightning, forest, food, wood and water 1 each (about 4%) | done | Read from Land 1's script; [../scripts/land1_script.md](../scripts/land1_script.md) |
| Land 2: fireball, lightning, heal, teleport, forest, food, storm, spiritual shield, physical shield, wood and water about 8% each; flying flock, and the creature's freeze, small, big, weak, strong, invisible, compassion, angry and itchy under 1% each; ground flock about 0.1% | done | Read from the land's script. Every land's script sets them (42 FIRE_FLY_SPELL_REWARD_PROB lines each); [magic.md](../../../magic/worship.md#dispensers-and-fireflies)'s "only Land 1" is wrong; [../scripts/land2_script.md](../scripts/land2_script.md) |
| Land 3: the same eleven at about 9% each and both flocks under 1%; no creature miracles | done | Read from the land's script. Every land's script sets them (42 FIRE_FLY_SPELL_REWARD_PROB lines each); [magic.md](../../../magic/worship.md#dispensers-and-fireflies)'s "only Land 1" is wrong; [../scripts/land3_script.md](../scripts/land3_script.md) |
| Land 4: the eleven, with stronger heal and stronger food, about 7% each; stronger fireball, lightning and water about 1.4%; strongest fireball and lightning and the storm with lightning about 0.7%; the nine creature miracles about 0.7% each; no flocks | done | Read from the land's script. Every land's script sets them (42 FIRE_FLY_SPELL_REWARD_PROB lines each); [magic.md](../../../magic/worship.md#dispensers-and-fireflies)'s "only Land 1" is wrong; [../scripts/land4_script.md](../scripts/land4_script.md) |
| Land 5: the eleven and stronger food about 6% each; both flocks about 3%; strongest fireball, stronger lightning, stronger heal and the nine creature miracles about 1.3%; stronger fireball, strongest lightning, the explosion miracle, the storm with lightning and the tornado about 0.6% | done | Read from the land's script. Every land's script sets them (42 FIRE_FLY_SPELL_REWARD_PROB lines each); [magic.md](../../../magic/worship.md#dispensers-and-fireflies)'s "only Land 1" is wrong; [../scripts/land5_script.md](../scripts/land5_script.md); see [../miracles/blast.md](../miracles/blast.md) |
| The tutorial land's weights are all zero, so a firefly caught there gives nothing | done | Read from the land's script; [../scripts/landT_script.md](../scripts/landT_script.md) |
| No table ever names the fat, thin, hungry, frightened, tired, ill or thirsty creature miracles, so fireflies never give them | done | Data-driven; see [../miracles/creature_spells.md](../miracles/creature_spells.md) |
| Skirmish: Two Gods and Four Gods share one table (fireball, lightning, heal, forest, food, storm, both shields, wood and water about 8% each, both flocks about 4%, teleport and the nine creature miracles about 1.5%, the explosion under 1%); Three Gods and Firestorm set every weight to zero, so their fireflies give nothing | done | Read from each map's script; see [../multiplayer/maps/](../multiplayer/maps/) |

## Saving and editing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps every firefly (its spot, whether it is out, hiding or flying, and its flight), the reward table, the limit of 50 and whether the next nightfall tops them up | pending | The saves' component reads and writes the firefly records (`FireFlyRecord`, `components/gamesave`), but a game loaded from the debug bar does not make its fireflies again; see [saved games](../../../engine/saved-games.md) |

## Hints in the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Land 2 "did you know" scroll: "Fireflies can be seen at night. They hide under rocks during the day, and become rewards if you find them." | partial 90% | The did-you-know scrolls are read when tapped (`src/Help/Bubble.cpp`); see [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md); no test |
