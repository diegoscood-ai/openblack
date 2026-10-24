# Splitting and heating rocks

Rocks and boulders can be broken in two by the hand, by the creature and by hard knocks, again and again, each half a
smaller rock of the same stone. They can also be heated: stone never burns away, so a rock stays as hot as whatever
heated it, glows, sets things alight around it and cools slowly. This file goes into the detail behind the rock rows of
[rocks_and_features.md](rocks_and_features.md), [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md),
[../physics/collisions.md](../physics/collisions.md) and [../physics/fire.md](../physics/fire.md); the fire model itself
(heat, spreading, cooling, water) is in the fire file.

openblack: `src/ECS/ObjectPhysics.cpp` (splitting and the tap), `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (the knocks),
`src/Fire/FireModel.cpp` and `src/ECS/Fire/FireEffect.cpp` (heat). The creature's blow is `object_physics::SmashRock`, from its knock-down in `CreatureObjectActionSystem.cpp`.

**Progress: 20/52 done, 0 chosen, 22 partial, 3 inferred — 74%**

## Which rocks split

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Everything the game's tables class as a rock can split: the three boulders and the flat, long, plain, sharp and square rocks in chalk, limestone, sandstone and volcanic stone, the scripts' plain rock and the meteor | done | `object_physics::IsRock` (the info's rock class, `src/ECS/ObjectPhysics.cpp`); scene `physics.rock_split` |
| Singing stones, the weeping stone, the idol, lanterns, gate totems, the altar and toys are other kinds and never split | done | `object_physics::IsRock`: only rows of the Rock kind in info.dat (the rock, the boulders and shaped rocks, the meteor) are rocks; `test/test_physics_objects.cpp` (`RockKinds.OnlyRowsOfTheRockKindAreRocks`, and `RockKinds.realInfoDatRockRows` on the game's rows) |
| Dead trees are handled like rocks in many ways but never split: a blow only lets a wood store take them, and they can't be tapped | done | A dead tree answers that it is no rock, so no knock, blow or tap splits it (`object_physics::IsRock`; `RockKinds.ADeadTreeIsNoRock` in `test/test_physics_objects.cpp`); a wood store takes them (`HandPhysics.cpp`, `HandApply` in `test/test_hand_apply.cpp`); see [trees.md](trees.md) |
| A bonfire is never worn by knocks or broken by a tap | partial 90% | No test; A bonfire is a mobile static of its own info (`BonfireArchetype`), not of the rock class |

## How a rock splits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rock is replaced by two rocks of the same kind, each 0.7935 of its scale (about half its volume, so half its weight), turned to its heading, at full life | done | Scene `physics.rock_split`; `object_physics::SplitRock` (the Y angle kept, new rocks at full life) |
| The halves lie either side of where it was, along a line at a random angle (from the game's random numbers), each 0.7935 of its width from the centre, at its height | done | Scene `physics.rock_split`; `object_physics::SplitRock` (a synced random angle, 0.7935 of its 2D radius, its altitude) |
| A flying rock's halves fly on at its speed with half its spin; a resting rock's halves start still and drop | done | Scene `physics.rock_split`; `ReactToPhysicsImpact` in `src/ECS/Physics/PhysicsObjects.cpp` passes them to `object_physics::SplitRock` |
| A flying rock the physics has lost track of doesn't split | partial 90% | No test; Only a rock in the physics list takes the impact |
| A burning or hot rock's halves both carry its heat | done | Tests `FirePassing.*`, scene `physics.burning_rock_split`; `fire::CopyFire` from `object_physics::SplitRock`; see [../physics/fire.md](../physics/fire.md) |
| The halves keep the rock's owner (the player who last threw it) | pending | The halves take the rock's `MobileStatic::owner` (`object_physics::SplitRock`, since 2026-10-19), but that owner is the player the rock was made for (a reward chest's, else the neutral player): a throw does not set it, so the last thrower is not kept; no test |
| The halves are new rocks: if the rock was a town's artefact, that is lost with it | partial 90% | `Rock::SplitInTwo` makes two new rocks (fn_00608770); the artefact goes with the split rock (from raffclar PR #278); no test; see [../town/artefacts.md](../town/artefacts.md) |
| Nothing limits how often a rock can be split; only the hand's tap and knocks stop at rocks 0.7 m tall or less | done | Test `PhysicsObjects.OnlyTallRocksBreakWhenTapped`; `object_physics::CanTapRock` and the wear test both need a height over 0.7 |
| A split from a knock has no dust, particles or sound of its own; only the tap and the creature's blow play a sound | partial 90% | No test; `ReactToPhysicsImpact` plays nothing; `object_physics::TapRock` plays the tap's sound |

## Knocks and landings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every turn, every rock in the physics that took a blow (from what it hit, what hit it, or the ground) is worn by it | done | Tests `PhysicsEndOfTurn.*`, scene `physics.rock_split`; `EndTurn` and `ReactToPhysicsImpact` in `src/ECS/Physics/PhysicsObjects.cpp` |
| A blow wears a rock only when it is harder than four times the rock's weight and the rock is taller than 0.7 m | done | Scene `physics.rock_split`; `ReactToPhysicsImpact` (G over 4, height over 0.7) |
| A rock is never worn by another rock, so rocks thrown at rocks bounce off whole | partial 90% | Test `PhysicsObjects.AKnockWearsATallRockHarderThanFourGUnlessFromAThingThatIsARock`; `object_physics::KnockRock` (not when the hitter answers it is a rock, `object_physics::IsRock`: a rock, the bonfire or a fragment, as the exe's 0x6E7982); not run in play |
| Each such blow takes (strength over its weight − 4) × 0.005 of its life; below 0.01 life it splits | done | Scene `physics.rock_split`; `ReactToPhysicsImpact` ((G - 4) x 0.005; below 0.01 it splits) |
| A rock thrown hard into the ground or a building can wear and split itself on landing | done | Scene `physics.rock_split`; Every body's blow is measured (`EndTurn`) |
| Miracles and everything else split rocks only through such blows, when something throws a rock about; no miracle, script or other rule splits a rock directly | done | Only the knock and the hand's tap split rocks in our tree |

## The hand and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a rock taller than 0.7 m in the player's influence splits it, with one of four cracking sounds in turn, and the player's creature sees the player playing | partial 70% | Test `PhysicsObjects.OnlyTallRocksBreakWhenTapped`; missing: the influence test and the creature's reaction; `object_physics::TapRock` from `HandSystem.cpp` (one of four cracking sounds in turn); the influence test before the tap and the creature's reaction are not there ([physics.md](../../../physics/physics.md#pending)); see [../hand/clicking_and_activating.md](../hand/clicking_and_activating.md) |
| The tooltip over a tappable rock says "hit to break" | partial 90% | No test; `HandToolTips.cpp` (the rock's own tap tooltip) |
| At the blow of its smash, the creature splits the rock it hits, with one of four smashing sounds in turn | partial 80% | The exe's `LH3DCreature::DoBlendedAction` calls `Rock::SplitInTwo`; scene `objects.smash_stone`; missing: the knock-down's choice of statics is not checked against v1.42; `object_physics::SmashRock` from the creature's knock-down (`CreatureObjectActionSystem::Moment`); its knock-down takes statics too (`CanDestroy`); the sound and its counter from the exe ([physics-impacts.md](../../../physics/physics-impacts.md#rocks-that-split)); unverified against v1.42 |
| The creature's blow on a villager or animal does nothing (the game's smash only breaks rocks and buildings) | partial 90% | No test; `CreatureObjectActionSystem::CanDestroy` refuses villagers and animals |
| The creature can choose to smash a stone in half by stamping on any rock | inferred | raffclar's port, not checked against the exe's `CreatureAgenda::ConstructSubActionsForSmashStoneInHalf`; plan action `SmashStoneInHalf` on any rock (`Target::Rock`), its blow at the right foot (testbed `objects.smash_stone`; raffclar PR #420, unverified against v1.42) |
| The creature can choose to break a rock: it picks up a rock it can lift, throws it at a spot three of its heights away, then goes to shatter it | inferred | raffclar's port, not checked against the exe's `CreatureAgenda::ConstructSubActionsForBreakRock`; plan action `BreakRock` (`creature_mind::BreakRock`): the step `Shatter` breaks the rock as it flies, the halves flying on (`object_physics::SplitRockAsItGoes`); whether the game breaks it in flight or where it lands is for the audit (testbed `objects.break_rock`; raffclar PR #420, unverified against v1.42) |
| A creature's blow or its shatter of a rock breaks the bonfire too, into two bonfires with their flames and no player of their own, and a piece knocked off a building into two plain rocks of the neutral player | partial 90% | `object_physics::IsRock` at the blow (`CreatureObjectActionSystem`) and the shatter (`CreatureMindSystem`), `object_physics::HalvesOf` and `SplitRock` through `MobileStaticArchetype::CreateFromInfo`, as the exe's `Rock::SplitInTwo` and its mobile static maker ([physics-impacts.md](../../../physics/physics-impacts.md#what-a-split-makes)); test `TakeResourceTest.ASplitThingsHalvesAreMadeFromItsRowWithItsPlayer`; not run in play, and a fragment is never a creature's target yet |

## Rocks as wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every rock holds wood: its table's wood value (200 for every rock and boulder) times its scale | done | `object_physics::RockWood` (`src/ECS/ObjectPhysics.cpp`), answered by `object_resources::GetResource`; the bonfire and the pieces knocked off buildings too; test `TakeResourceTest.RocksHoldWoodByTheirScale` |
| Taking wood from a rock shrinks it by the share left, and brings its height above the ground down with it (its life stays); a rock with all its wood taken, or asked for anything but wood, is gone | done | `object_physics::TakeFromRock` (`src/ECS/ObjectPhysics.cpp`), reached from `object_resources::RemoveResource`; test `test/test_take_resource.cpp` with the original's numbers |
| Nothing in the game takes wood from a plain rock: villagers only go for wood to dead trees, which share this rule, and rocks are not stores | inferred | Not traced; our rocks now hold and give wood as the original's do (`object_physics::RockWood`, `TakeFromRock`), reached only through `object_resources` |

## Heating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every rock and boulder burns at 500 degrees, holds heat like 4000 units, and is never hurt by fire | done | Scene `physics.burning_rock_split`; Read from info.dat (`src/ECS/Fire/FireObjectTraits.cpp`) |
| Stone never heats itself: even at 500 or more a rock does not burn hotter, it only cools whenever nothing heated it that turn | done | Test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain`; `fire::Step` (`src/Fire/FireModel.cpp`): an object with no burn defence only cools |
| A rock gets only as hot as what heats it: burning trees (at most 220 degrees) and huts (300) can't bring it to 500; the fireball (up to 6000), lightning and scripts can | done | Test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat`; The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`, the heat capped by the difference) |
| A fireball's ball and its burn heat a rock like any other object | done | Test `FireModel.ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn`; `src/Magic/Objects/MagicFireBall.cpp`, `fire::ApplyEffectToFireEffectIfNecessary` (over `fire::ApplyBurn`, `src/Fire/FireModel.cpp`) |
| A rock held in the hand is heated by the fires in its cell, but only while the hand is inside its player's influence | partial 90% | No test; `fire::CheckToSeeIfObjectIsNearOnFireObject` from `HandTurn.cpp`, inside the holder's influence |
| A hot rock held in the hand passes its heat on only inside the holder's influence | partial 90% | No test; The spread of a held object in `src/ECS/Fire/FireEffect.cpp` (only inside the holder's influence) |
| The hand is never hurt by a hot rock and never drops it | done | Nothing ties the hand to heat |
| From 400 degrees flames grow on a rock, full at 1000; at 500 or more it is on fire | done | `fire::FireFraction`, `fire::IsOnFire` (`src/Fire/FireModel.cpp`), `src/ECS/Fire/FireGraphic.cpp`; `FireModel.ARockFlamesFromFourHundredFullAtAThousandAndBurnsAtFiveHundred` in `test/test_fire_model.cpp` |
| Below 500 a hot rock heats only things that catch at or below its temperature (villagers 120, trees 110, huts 150, fences 300) and loses the heat it gives; at 500 or more it heats everything within reach without losing any | done | Test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat`; The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`) and the spread (`src/ECS/Fire/FireEffect.cpp`) |
| A thrown hot rock sets alight what it lands among by the same rules, its reach being its fire's radius plus 10 m | partial 90% | No test; Flying things are off the map for fire; once at rest it spreads again (`src/ECS/Fire/FireEffect.cpp`); see [../physics/fire.md](../physics/fire.md) |
| From 100 degrees a rock makes villagers within 35 m react as to a fire | done | The fire's reaction (`src/ECS/Fire/FireEffect.cpp`, `fire::IsAboveReactionTemperature`); `FireModel.ARockMakesPeopleReactFromAHundredDegrees` in `test/test_fire_model.cpp`, the 35 m in `MagicTables.realInfoDat` |
| Villagers flee a burning rock carried in the hand | partial 30% | Missing: the villagers' handler for the reaction; The reaction is made, but villagers have no handler for it; see [../physics/fire.md](../physics/fire.md) |
| In the open a rock cools by its surface over its 4000 units of heat, so a big rock stays hot for minutes; rain cools it faster | done | Test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain`; `fire::Step` and `fire::CoolingArea` (`src/Fire/FireModel.cpp`) |
| Dropped in water (or on the shore, under 2 m up) it cools fifty times faster, hissing with steam once while hotter than 75 | partial 70% | Missing: the hiss sound; Fifty times faster (`fire::Step`, `src/Fire/FireModel.cpp`) and the steam above 75 (`src/ECS/Fire/FireGraphic.cpp`); no hiss sound in our tree |
| A hot rock glows red-orange, a thousandth of full per degree with a 20% flicker, full at 1000 | partial 80% | `graphic::CharringGlow` (`src/ECS/Fire/FireGraphic.cpp`); missing: the flicker's noise is approximated; no test of the glow |
| A burning rock crackles like any fire, the two fires nearest the camera holding the loop | partial 90% | `src/ECS/Fire/FireSound.cpp`, as [the wiki](../../../world/fire.md#fire): the slot's former fire stops its sound as a fire takes the slot (test `FireSound.AFireRetakingItsOwnSlotStopsItsCrackleFirst`); the slots' choice in play has no test |
| A script can ask whether something is on fire near a place; a rock at 500 or more counts | done | `script::IsFireNear` (`src/Magic/Script/CHLFire.cpp`, `FireSystem::IsFireNear`); `FiresOnTheMap.AFireNearAPlaceIsAThingOnFireWithinTheRadius` in `test/test_fire.cpp` |

## The creature and hot rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature can set something alight with a burning thing it can lift (a burning rock among them): it picks it up, goes to the target, faces it, throws it down there and waits | pending | See [../creature/object_actions.md](../creature/object_actions.md) |
| The creature's own "start a fire" action always gives up at once | pending | Not in our tree |

## Scripts with hot rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 2's lost-treasure riddle asks for "something hot": it is solved when a wolf and a creature's dropping are within 10 m of the altar, a shield miracle within 5 m, and anything on fire (a rock at 500 or more will do) within 10 m | pending | IS_FIRE_NEAR is done; the challenge itself is not checked running; see [../story/](../story/) and [the_riddles.md](../story/silver_scrolls/the_riddles.md) |
| Land 5: while the player's creature is within 200 m of the front of Nemesis's last town, every 6 seconds a burning rock (half size, 2000 degrees, not blown by the wind) is lobbed from high above to land within 30 m of a spot in front of the town over 6 seconds, trailing smoke | pending | Story script; see [../story/](../story/) |
| Each such rock that comes to rest (not caught in the hand) a few seconds later bursts: a small fireball is cast where it lies and the rock goes in an explosion | pending | Story script; see [../story/](../story/) |
| The "fire on high" challenge throws the same burning rocks at a town, and praises a good catch by the hand | pending | Challenge script; see [../story/](../story/) |
