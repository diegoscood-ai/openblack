# Fire

Anything can be heated. Each heated object holds a temperature; at its burning point it catches, burns hotter, loses
life, chars and heats everything within reach, so fire spreads from tree to tree and house to house until it burns out,
is beaten out by villagers, or is cooled by water or rain. Villagers flee, fight or skirt it; those it reaches run
burning. The miracles that start fires have their own pages: [../miracles/fireball.md](../miracles/fireball.md),
[../miracles/lightning.md](../miracles/lightning.md), [../miracles/storm.md](../miracles/storm.md),
[../miracles/blast.md](../miracles/blast.md); putting it out with water is in
[../miracles/water.md](../miracles/water.md).

openblack: `src/ECS/Fire/FireEffect.cpp` (the fire list, spread, groups), `src/ECS/Fire/FireSound.cpp` (sound),
`src/Fire/FireModel.cpp` (temperature, heat, damage, charring), `src/ECS/Fire/FireGraphic.cpp` (flames, steam, smoke),
`src/ECS/Systems/Implementations/VillagerFire.cpp` (villagers), tests in `test/test_fire.cpp` and
`test/test_fire_model.cpp`.

**Progress: 62/77 done, 10 partial — 87%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Heat and burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every object hotter than the air carries a fire record with its temperature; the air is about 24.7 degrees everywhere | done | `fire::Create` (`src/ECS/Fire/FireEffect.cpp`) and the model's `fire::k_AmbientTemperature` of 24.7 (`src/Fire/FireModel.h`) |
| Each kind of object has a burning point (at least 40), a heat capacity and a burn defence from the game's tables | done | `FireEffect::CombustionThreshold` and `Capacity` over the model's `fire::CombustionTemperature` and `fire::Capacity` (`src/Fire/FireModel.cpp`: at least 40, at least 1), the info rows read by `src/ECS/Fire/FireObjectTraits.cpp`; test `FireModel.NothingCatchesBelowFortyDegreesAndHoldsAtLeastOneUnitOfHeat` |
| Typical materials: villagers catch at 120, animals 140, trees and bushes 110, dead trees 100, huts 150, civic buildings 160–200, wonders 250, creatures 200, fences 300 | done | Data-driven: info.dat through `src/ECS/Fire/FireObjectTraits.cpp` |
| Rocks, stones, toys, flowers and bonfires have no burn defence: they glow and spread heat but are never hurt | done | Data-driven (their burn defence is 0, so they heat but take no harm); see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Features (pyramids, pillars) burn only at 2000 with a huge capacity, so they are fireproof in practice | done | Data-driven |
| At or above its burning point an object burns; it heats itself by a tenth of its heat over twice its burning point each turn, up to twice its burning point | done | `fire::Step` (`src/Fire/FireModel.cpp`: T += 0.1 T / 2 Tc up to 2 Tc), run by the fire's turn in `src/ECS/Fire/FireEffect.cpp`; test `FireModel.ABurningThingHeatsItselfSlowlyAndHurts` |
| A burning object loses life each turn by how far it is above its burning point, times its burn defence (a villager at full heat dies in about 20 turns; a tree lasts about 1000) | done | `fire::BurnDamage` from `fire::Step` (`src/Fire/FireModel.cpp`), applied by `fire::traits::ReduceLifeDueToBurning` from `src/ECS/Fire/FireEffect.cpp` (creatures excepted, see below); test `FireModel.BurningAtTwiceItsCombustionHurtsByItsDefence` |
| Above three times its burning point it is "very hot" and bursts into extra flames | done | `fire::Step` sets the very-hot flag above 3 Tc (`src/Fire/FireModel.cpp`, `FireEffect::k_VeryHot`); extra flames in `src/ECS/Fire/FireGraphic.cpp` |
| A fire's strength (size of flames, reach, sound) grows from 0.8 of the burning point to twice it, and shrinks as the object's life runs out | done | `fire::FireFraction` (limited by twice the life), `FireRadius` and `FlameHeight` (`src/Fire/FireModel.cpp`), behind `FireEffect`'s methods of the same names; test `FireModel.TheFireIsFiercestAtTwiceItsCombustionAndWeakAsItsObjectBurnsAway` |
| An object not burning cools by its surface over its capacity, faster the hotter it is | done | `fire::Step` and `fire::CoolingArea` (`src/Fire/FireModel.cpp`: the 4 H r area, the water and rain multipliers); test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| The fire record is dropped once the object is back within 0.1 degrees of the air and has no charring left | done | `fire::Step` reports it gone (`src/Fire/FireModel.cpp`: within 0.1 degrees of the air with no charring left) and `src/ECS/Fire/FireEffect.cpp` drops the record; test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| Below 0.6 life a burning object chars; when the fire is out the charring slowly fades back to what its life allows | done | `fire::Step` (`src/Fire/FireModel.cpp`: +0.04 a turn up to its cap, -0.02 a turn once out); test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| A burn from a miracle or a villager's beating pulls the temperature towards the air's plus the burn | done | `fire::ApplyEffectToFireEffectIfNecessary` (`src/ECS/Fire/FireEffect.cpp`) over the model's `fire::ApplyBurn` (`src/Fire/FireModel.cpp`), called by `EffectValues.cpp` and `VillagerFire.cpp`; test `FireModel.ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn` |
| Scripts can set an object on fire, set its temperature, ask if it is on fire or a fire is near, and stop it being hurt or set alight | done | `src/Magic/Script/CHLFire.cpp` (SET_ON_FIRE, SET_TEMPERATURE, IS_ON_FIRE, IS_FIRE_NEAR, SET_HURT_BY_FIRE, SET_SET_ON_FIRE) |
| A fire set by a script, or by an object's own owner, is credited to that object's owner | done | `fire::SetTemperature` and `fire::SetOnFire` credit the object's own player |
| Fires are processed once a turn, newest first, after the living things and before the scripts and miracles | done | `fire::ProcessList` from `src/Magic/MagicLoop.cpp`; fires made while the list runs wait for the next turn |

## Catching and spreading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning object heats every object in the cells within its reach (its fire radius plus 10 m), searched outward in a spiral | done | The spread in `src/ECS/Fire/FireEffect.cpp` (spiral of cells within the fire radius plus 10 m) |
| It reaches a target only when their distance is less than the target's own size plus the fire's radius (up to 1.25 × the object's size) | done | `HeatTransfer` in `src/ECS/Fire/FireEffect.cpp` (the table distance) |
| Tall fires and tall targets also need to overlap in height | done | `HeatTransfer` (only when either is 3 m or more above the land) |
| The heat passed is ten times the temperature difference, at most half the burning object's stored heat | done | The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`: ten a degree, at most half the source's heat), from `HeatTransfer` in `src/ECS/Fire/FireEffect.cpp`; test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat` |
| A fire not yet burning only passes heat to things that catch at or below its temperature, and loses what it gives | done | The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`: a source not on fire loses what it gives); test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat` |
| A fire never heats the thing that heated it | done | `HeatTransfer` (`FireEffect::source`) |
| The wind does not steer fire: the game works the wind out and throws it away | done | No wind in the spread (`src/ECS/Fire/FireEffect.cpp`) |
| Fires that touch join one blaze; the blaze's head owns the list of villagers fighting it, and hands over to another hot member when it cools | done | The fire groups (root, next, the root's firemen list, the hand-over) in `src/ECS/Fire/FireEffect.cpp` |
| A dead tree's reach is 0.35 of its height; everything else uses its width | done | `fire::traits::DefaultFireRadius`, `FireCentre` (`src/ECS/Fire/FireObjectTraits.cpp`) |
| Fields burn their food instead of their life, even when scripts said they are not hurt by fire | done | `fire::traits::ReduceLifeDueToBurning` (a field's food goes before the not-hurt test) |
| Trees catch from neighbouring trees, so whole forests burn | done | The spread in `src/ECS/Fire/FireEffect.cpp` |
| Huts catch from burning villagers and trees beside them, and villagers from burning huts | done | The spread; a villager reached starts burning (`villager_fire::SetupOnFire`) |
| Fires start from the fireball, lightning (miracle and storm), the blast's heat and scripts | done | `src/Magic/Objects/MagicFireBall.cpp`, `src/Particles/Rules/Lightning.cpp`, `Storm.cpp`, `Explosion.cpp`, `src/Magic/Script/CHLFire.cpp` |
| A thing held in the hand over a fire catches from the fires in its cell | done | `fire::CheckToSeeIfObjectIsNearOnFireObject` from `HandTurn.cpp`, only inside the holder's influence. See [../hand/holding.md](../hand/holding.md) |
| Picking up a burning object takes it out of its blaze; villagers flee a burning object carried in the hand (reach 30, running 20–50 m) | partial | `fire::StartedMoving` from `HandHolding.cpp` takes it out of its group and gives it the burning-object-in-hand reaction, but no villager reacts to that reaction |
| A burning object flying through the air keeps burning and heats villagers fighting fires | done | `fire::StartedMoving` when it starts physics (`src/ECS/Physics/PhysicsObjects.cpp`); flying things are off the map for fire |
| When a rock splits, both halves keep its fire | done | `Rocks::SplitInTwo` calls `fire::CopyFire` (`src/ECS/Rocks.cpp`); see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| A bonfire burns for ever and never hurts | done | `BonfireArchetype` (`src/ECS/Archetypes/BonfireArchetype.cpp`): a rock with an endless bonfire spot visual and no fire |
| Villagers hidden in a building, at home, held or dying never catch | partial | Hidden and held villagers are out of the map cells, so the spread does not find them; there is no own test for dying villagers |

## Putting fires out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In water (a water or shore cell, under 2 m up) a fire cools fifty times faster, so burning villagers and things that reach the sea go out | done | `fire::Step` (`src/Fire/FireModel.cpp`: fifty times in the water); the water or shore cell under 2 m is tested in `src/ECS/Fire/FireEffect.cpp`; test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| Rain or snow cools fires faster (up to 2.27 times in the heaviest rain) | done | `fire::Step` (`src/Fire/FireModel.cpp`: the rain cooling multiplier times the rain or snow there, read out of the water in `src/ECS/Fire/FireEffect.cpp`) |
| Rain putting out a fire makes villagers stop to watch the storm, once | done | `src/ECS/Fire/FireEffect.cpp` (when `fire::Step` reports the rain cooled it) and `src/Magic/Spells/SpellStormAndTornado.cpp` (the storm's watch reaction, once) |
| The water miracle's drops cool what they hit by 4000 degrees each | done | `src/Magic/Spells/SpellWater.cpp`. See [../miracles/water.md](../miracles/water.md) |
| A burning villager does not look for water; it runs about at random until its fire dies | done | `villager_fire::OnFire` (`src/ECS/Systems/Implementations/VillagerFire.cpp`) |
| A fire is out when it drops below its burning point; the object then smokes for 30 turns where it stands | done | The smoke bursts of 30 turns in `src/ECS/Fire/FireGraphic.cpp` |
| A fire still hotter than 75 degrees that is being cooled gives off steam with a hiss, once per cooling | partial | The steam (cooling and above 75) is in `src/ECS/Fire/FireGraphic.cpp`; no hiss sound in our tree, and our wiki does not describe one |

## Villagers and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fire hotter than 100 (or burning) makes villagers within 35 m react to it, at high priority; the reaction is removed when it cools | done | The fire's reaction in `src/ECS/Fire/FireEffect.cpp`, `villager_fire::ReactToFirePriority` |
| A reacting villager first looks at the fire, and runs 10 m away if it is close or within the fire's safe distance | done | `villager_fire::ReactToFire` |
| Villagers of the fire's own town decide whether to fight it, by how big the blaze is, how many already fight it, its importance and how far they are from home | partial | `villager_fire::ReactToFire` has the score, but its inputs are approximated and the decision is marked unverified in the code |
| Villagers on their way to worship don't join in | done | `villager_fire::ReactToFire` tests `WorshipVillager::onWay`, set by `VillagerWorship.cpp` |
| Fire fighters beat the flames from 2 m, cooling it by 8 degrees a blow, and are immune to its heat | done | `villager_fire::PutOutFireByBeating`; firemen are immune in `HeatTransfer` |
| Villagers never fetch water to fight a fire; those states exist but are never chosen | done | `villager_fire::PutOutFireWithWater` gives up at once |
| Villagers that won't fight go round the fire on the side they lean to | done | `villager_fire::MoveAroundFire`; no unit test in our tree |
| A villager the heat reaches runs: a burning one 4–10 m in random directions while it burns, then goes back to what it was doing | done | `villager_fire::SetupOnFire`, `OnFire` |
| A burning villager dies when its life runs out, credited to whoever made the fire | done | `fire::traits::DestroyedByEffect` to `villager::DestroyedByEffect` with the fire's player |
| A villager leaving a fire reaction always ends up in a sensible state | done | `villager_reactions::ReactionValidate` and `villager_fire::ShutDownReaction` |
| The town counts injured villagers as fire hurts them | todo | No count of injured villagers in our tree |
| Animals react to fire and flee it | todo | Animals do not take the fire reaction yet ([animals.md](../../bw1-notes/animals.md)) |
| Burning animals die (fall and lie) rather than vanish | done | `fire::traits::DestroyedByEffect` to `animal_ai::DestroyedByEffect` (dying) |

## The creature and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can catch fire; it burns very slowly (it takes about 500 turns of full heat to lose all its life) and faints rather than dies | partial | A creature gets a fire record, but `fire::traits::ReduceLifeDueToBurning` gives it no harm (not ported) |
| A creature that catches gets burn marks on its skin, not flames | todo | No burn marks and no flames on a creature (`src/ECS/Fire/FireEffect.cpp` TODO). Our wiki differs: the original gives a burning creature three flames at random points of its box ([magic.md](../../bw1-notes/magic.md#fire-srcecsfire)) |
| A creature reacts to fires near it, can put fires out, set things on fire, and stop itself burning (with the water miracle if it knows it) | todo | Only its mind's beliefs see fire (`CreatureMindLearning.cpp`); see [../creature/](../creature/) |
| A creature's warmth comes from the climate only: fires don't warm it | done | `CreaturePhysiologySystem.cpp` reads the weather's temperature; see [../creature/desires.md](../creature/desires.md) |

## Buildings, trees and fields burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning building empties: its people come out | done | `abodes::ReduceLife` (`src/ECS/Abodes.cpp`): every inhabitant reacts as to a tap, the building stops working |
| A building burnt to nothing flickers out as a ghost and goes; buildings a script holds stay as a building site | partial | `abodes::ReduceLife` makes the building site; `fire::traits::DestroyedByEffect` does nothing for an abode yet (no ghost). See [../building/](../building/) |
| A burning tree darkens, its leaves thin as it heats, and below 0.2 life it narrows (but keeps its height) until it is gone | partial | Darkening (`graphic::TreeDrawColour`) and narrowing below 0.2 life (`RenderingSystem.cpp`) are done; the thinning of the leaves is not ported |
| A tree burnt away is simply removed: there is no burnt-tree stump left | done | `fire::traits::DestroyedByEffect` deletes it |
| A burnt field loses its crop and its fire | done | `fire::traits::DestroyedByEffect` (crops, growth and food to 0, the fire deleted) |
| A burning field never lights the land around it | done | `graphic::Create` gives the glow only to multi-cell fixed objects over 2 m. Our wiki differs: every abode is such an object, a field included, so a large burning field does light the land ([magic.md](../../bw1-notes/magic.md#fire-srcecsfire)) |
| Things that catch lose the reactions they gave (a pot's food, a dead tree's wood) and get them back when the fire ends | partial | `fire::traits::StartOnFire` and `EndOnFire` (`FireObjectTraits.cpp`): dead trees and magic trees get theirs back; a pot does not (TODO) |

## Look and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flames: 2 on trees, 7 on things 3 m or bigger, else 2; more grow in as the fire strengthens; each lives 4.3 s, fading in over 1 s | done | `src/ECS/Fire/FireGraphic.cpp` (most flames, 4.3 s life, 1 s fade-in) |
| Flames sit at random points on the model (on trees in the lower half; on moving things on their bones) | partial | A random triangle of the mesh, trees in the lower half (`LocalRandomFlamePosition`); moving things' posed bones and the damaged building model are not used |
| Flame size: 0.2 of the height on trees, 0.3 on the creature and the temple, 0.5 on everything else | partial | Trees 0.2 and the rest 0.5 (`src/ECS/Fire/FireGraphic.cpp`); the 0.3 of the larger objects is not ported |
| A fire long out of sight catches up its flames when seen again | done | Our tree updates every fire every frame, drawn or not, so there is nothing to catch up |
| Burning buildings and objects darken as they char and glow red-orange with heat | done | `graphic::CharringGrey`, `CharringGlow` (the glow's noise is approximated); test `FireGraphic.CharringGreyMatchesTheExe` |
| Big burning objects light the land around them with a flickering glow | done | `src/ECS/Fire/FireGraphic.cpp` (the fire light map on multi-cell fixed objects over 2 m) |
| Smoke and steam puffs drift with the smoothed wind and rise | done | `src/ECS/Fire/FireGraphic.cpp` |
| Only one fire crackle loop plays, on the burning fire nearest the camera on the ground; a farther fire never takes it over | done | `src/ECS/Fire/FireSound.cpp` plays the two nearest fires. Our wiki differs: two fires nearest the camera, with a fraction above 0.1, make sound ([magic.md](../../bw1-notes/magic.md#fire-srcecsfire)) |
| The crackle stops when its fire weakens below a tenth | done | `sound::Consider` (`src/ECS/Fire/FireSound.cpp`) |
| There is no catching, going-out or collapse sound | done | None in our tree |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fires (temperature, charring, blaze, fighters) are saved and loaded with the game | todo | openblack has no saved games |
