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

**Progress: 17/79 done, 0 chosen, 57 partial, 1 inferred — 81%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## Heat and burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every object hotter than the air carries a fire record with its temperature; the air is about 24.7 degrees everywhere | done | `fire::Create` (`src/ECS/Fire/FireEffect.cpp`) and the model's `fire::k_AmbientTemperature` of 24.7 (`src/Fire/FireModel.h`); test `FireModel.ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn` |
| Each kind of object has a burning point (at least 40), a heat capacity and a burn defence from the game's tables | done | `FireEffect::CombustionThreshold` and `Capacity` over the model's `fire::CombustionTemperature` and `fire::Capacity` (`src/Fire/FireModel.cpp`: at least 40, at least 1), the info rows read by `src/ECS/Fire/FireObjectTraits.cpp`; test `FireModel.NothingCatchesBelowFortyDegreesAndHoldsAtLeastOneUnitOfHeat` |
| Typical materials: villagers catch at 120, animals 140, trees and bushes 110, dead trees 100, huts 150, civic buildings 160–200, wonders 250, creatures 200, fences 300 | partial 90% | Data-driven: info.dat through `src/ECS/Fire/FireObjectTraits.cpp`; no test or check covers it (no test reads the rows' values) |
| Rocks, stones, toys, flowers and bonfires have no burn defence: they glow and spread heat but are never hurt | partial 90% | Data-driven (their burn defence is 0, so they heat but take no harm); no test or check covers it; see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Features (pyramids, pillars) burn only at 2000 with a huge capacity, so they are fireproof in practice | partial 90% | Data-driven; no test or check covers it |
| At or above its burning point an object burns; it heats itself by a tenth of its heat over twice its burning point each turn, up to twice its burning point | done | `fire::Step` (`src/Fire/FireModel.cpp`: T += 0.1 T / 2 Tc up to 2 Tc), run by the fire's turn in `src/ECS/Fire/FireEffect.cpp`; test `FireModel.ABurningThingHeatsItselfSlowlyAndHurts` |
| A burning object loses life each turn by how far it is above its burning point, times its burn defence (a villager at full heat dies in about 20 turns; a tree lasts about 1000) | done | `fire::BurnDamage` from `fire::Step` (`src/Fire/FireModel.cpp`), applied by `fire::traits::ReduceLifeDueToBurning` from `src/ECS/Fire/FireEffect.cpp` (a creature loses its body's life too, see below); test `FireModel.BurningAtTwiceItsCombustionHurtsByItsDefence` |
| Above three times its burning point it is "very hot" and bursts into extra flames | partial 70% | `fire::Step` sets the very-hot flag above 3 Tc (`src/Fire/FireModel.cpp`), extra flames in `src/ECS/Fire/FireGraphic.cpp`; the threshold is not in our wiki's reading of the exe ([fire.md](../../../world/fire.md#the-heat-model)), and the tests only check the flag is not set below it |
| A fire's strength (size of flames, reach, sound) grows from 0.8 of the burning point to twice it, and shrinks as the object's life runs out | done | `fire::FireFraction` (limited by twice the life), `FireRadius` and `FlameHeight` (`src/Fire/FireModel.cpp`), behind `FireEffect`'s methods of the same names; test `FireModel.TheFireIsFiercestAtTwiceItsCombustionAndWeakAsItsObjectBurnsAway` |
| An object not burning cools by its surface over its capacity, faster the hotter it is | done | `fire::Step` and `fire::CoolingArea` (`src/Fire/FireModel.cpp`: the 4 H r area, the water and rain multipliers); test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| The fire record is dropped once the object is back within 0.1 degrees of the air and has no charring left | done | `fire::Step` reports it gone (`src/Fire/FireModel.cpp`: within 0.1 degrees of the air with no charring left) and `src/ECS/Fire/FireEffect.cpp` drops the record; test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| Below 0.6 life a burning object chars; when the fire is out the charring slowly fades back to what its life allows | done | `fire::Step` (`src/Fire/FireModel.cpp`: +0.04 a turn up to its cap, -0.02 a turn once out); test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| A burn from a miracle or a villager's beating pulls the temperature towards the air's plus the burn | done | `fire::ApplyEffectToFireEffectIfNecessary` (`src/ECS/Fire/FireEffect.cpp`) over the model's `fire::ApplyBurn` (`src/Fire/FireModel.cpp`), called by `EffectValues.cpp` and `VillagerFire.cpp`; test `FireModel.ABurnPullsTheTemperatureTowardsTheAirsPlusTheBurn` |
| Scripts can set an object on fire, set its temperature, ask if it is on fire or a fire is near, and stop it being hurt or set alight | partial 90% | `src/Magic/Script/CHLFire.cpp` (SET_ON_FIRE, SET_TEMPERATURE, IS_ON_FIRE, IS_FIRE_NEAR, SET_HURT_BY_FIRE, SET_SET_ON_FIRE), as [CHL natives](../../../world/fire.md#chl-natives); no test or check covers it |
| A fire set by a script, or by an object's own owner, is credited to that object's owner | partial 90% | `fire::SetTemperature` and `fire::SetOnFire` credit the object's own player; no test or check covers it |
| Fires are processed once a turn, newest first, after the living things and before the scripts and miracles | partial 90% | `FireSystem::ProcessTurn` (`ecs::fire::ProcessList`) at slot 6 of `src/Magic/MagicLoop.cpp`, after the living and before the reactions and spells; fires made while the list runs wait for the next turn; no test or check covers it |

## Catching and spreading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning object heats every object in the cells within its reach (its fire radius plus 10 m), searched outward in a spiral | partial 90% | The spread in `src/ECS/Fire/FireEffect.cpp` (spiral of cells within the fire radius plus 10 m, the cell's objects in `FindType` order); no test or check covers it |
| It reaches a target only when their distance is less than the target's own size plus the fire's radius (up to 1.25 × the object's size) | partial 90% | `HeatTransfer` in `src/ECS/Fire/FireEffect.cpp` (the table distance); no test or check covers it |
| Tall fires and tall targets also need to overlap in height | partial 70% | `HeatTransfer` (only when either is 3 m or more above the land); no test or check covers it, and the height test is not in our wiki's reading of the exe |
| The heat passed is ten times the temperature difference, at most half the burning object's stored heat | done | The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`: ten a degree, at most half the source's heat), from `HeatTransfer` in `src/ECS/Fire/FireEffect.cpp`; test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat` |
| A fire not yet burning only passes heat to things that catch at or below its temperature, and loses what it gives | done | The model's `fire::HeatTransfer` (`src/Fire/FireModel.cpp`: a source not on fire loses what it gives); test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat` |
| A fire never heats the thing that heated it | partial 90% | `HeatTransfer` (`FireEffect::source`); no test or check covers it |
| The wind does not steer fire: the game works the wind out and throws it away | done | No wind in the spread (`src/ECS/Fire/FireEffect.cpp`) |
| Fires that touch join one blaze; the blaze's head owns the list of villagers fighting it, and hands over to another hot member when it cools | partial 80% | The fire groups (root, next, the root's firemen list, the hand-over) in `src/ECS/Fire/FireEffect.cpp`; no test or check covers it (the `FirePassing` tests cover only a copied fire joining the blaze) |
| A dead tree's reach is 0.35 of its height; everything else uses its width | partial 70% | `fire::traits::DefaultFireRadius`, `FireCentre` (`src/ECS/Fire/FireObjectTraits.cpp`); the 0.35 is not in our wiki's reading of the exe (only the dead tree's fire centre is); no test or check covers it |
| Fields burn their food instead of their life, even when scripts said they are not hurt by fire | partial 90% | `fire::traits::ReduceLifeDueToBurning` (a field's food goes before the not-hurt test), as [What each class does](../../../world/fire.md#what-each-class-does-with-fire); no test or check covers it |
| Trees catch from neighbouring trees, so whole forests burn | partial 90% | The spread in `src/ECS/Fire/FireEffect.cpp`; no test or check covers it |
| Huts catch from burning villagers and trees beside them, and villagers from burning huts | partial 90% | The spread; a villager reached starts burning (`villager_fire::SetupOnFire`); no test or check covers it |
| Fires start from the fireball, lightning (miracle and storm), the blast's heat and scripts | partial 90% | `src/Magic/Objects/MagicFireBall.cpp`, `src/Particles/Rules/Lightning.cpp`, `Storm.cpp`, `Explosion.cpp`, `src/Magic/Script/CHLFire.cpp`; the fireball is in the hand demo `fwd_fireball` and the scene `miracles.hand_fireball_throws`, the other starts have no test or check |
| A thing held in the hand over a fire catches from the fires in its cell | partial 90% | `fire::CheckToSeeIfObjectIsNearOnFireObject` from `HandTurn.cpp`, only inside the holder's influence; no test or check covers it. See [../hand/holding.md](../hand/holding.md) |
| Picking up a burning object takes it out of its blaze; villagers flee a burning object carried in the hand (reach 30, running 20–50 m) | partial 50% | `fire::StartedMoving` from `HandHolding.cpp` takes it out of its group and gives it the burning-object-in-hand reaction; no villager reacts to that reaction, and no test |
| A burning object flying through the air keeps burning and heats villagers fighting fires | partial 90% | `fire::StartedMoving` when it starts physics (`DynamicsSystem::Add`, `src/ECS/Systems/Implementations/DynamicsSystem.cpp`, and a particle system's take, `src/ECS/Physics/ParticleCarriedObjects.cpp`); flying things are off the map for fire; no test or check covers it |
| When a rock splits, both halves keep its fire | done | `object_physics::SplitRock` calls `fire::CopyFire` (`src/ECS/ObjectPhysics.cpp`); tests `FirePassing.ACopiedFireJoinsTheBlazeAndTakesTheHotterTemperature`, `FirePassing.AFireIsNotCopiedOntoAnObjectWithNoInfo`; see [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| A bonfire burns for ever and never hurts | partial 90% | `BonfireArchetype` (`src/ECS/Archetypes/BonfireArchetype.cpp`): a rock with an endless bonfire spot visual and no fire, as bw1-decomp's `Bonfire` (a `Rock`); no test or check covers it |
| Villagers hidden in a building, at home, held or dying never catch | partial 60% | Hidden and held villagers are out of the map cells, so the spread does not find them; nothing keeps a dying villager from catching, and no test |

## Putting fires out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In water (a water or shore cell, under 2 m up) a fire cools fifty times faster, so burning villagers and things that reach the sea go out | done | `fire::Step` (`src/Fire/FireModel.cpp`: fifty times in the water); the water or shore cell under 2 m is tested in `Process` (`src/ECS/Fire/FireEffect.cpp`); test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| Rain or snow cools fires faster (up to 2.27 times in the heaviest rain) | done | `fire::Step` (`src/Fire/FireModel.cpp`: the rain cooling multiplier times the rain or snow there, read out of the water in `src/ECS/Fire/FireEffect.cpp`); test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| Rain putting out a fire makes villagers stop to watch the storm, once | partial 90% | `src/ECS/Fire/FireEffect.cpp` (when `fire::Step` reports the rain cooled it) and `src/Magic/Spells/SpellStormAndTornado.cpp` (the storm's watch reaction, once); no test or check covers it |
| The water miracle's drops cool what they hit by 4000 degrees each | partial 90% | `src/Magic/Spells/SpellWater.cpp`; no test or check covers it. See [../miracles/water.md](../miracles/water.md) |
| A burning villager does not look for water; it runs about at random until its fire dies | partial 90% | `villager_fire::OnFire` (`src/ECS/Systems/Implementations/VillagerFire.cpp`); no test or check covers it |
| A fire is out when it drops below its burning point; the object then smokes for 30 turns where it stands | partial 90% | The smoke bursts of 30 turns in `src/ECS/Fire/FireGraphic.cpp`; no test or check covers it |
| A fire still hotter than 75 degrees that is being cooled gives off steam with a hiss, once per cooling | partial 90% | The steam (cooling and above 75, hotter than at the last burst: `fire::graphic::StartsSteam`) and the sizzle (InGame sample 0x35, G_Steam_01) are in `src/ECS/Fire/FireGraphic.cpp`, as [Steam and smoke](../../../world/fire.md#steam-and-smoke); test `FireGraphic.ACooledFireAboveSeventyFiveDegreesSteams` (the sizzle itself: no test) |

## Villagers and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fire hotter than 100 (or burning) makes villagers within 35 m react to it, at high priority; the reaction is removed when it cools | partial 90% | The fire's reaction in `src/ECS/Fire/FireEffect.cpp`, `villager_fire::ReactToFirePriority`; no test or check covers it |
| A reacting villager first looks at the fire, and runs 10 m away if it is close or within the fire's safe distance | partial 90% | `villager_fire::ReactToFire`; no test or check covers it |
| Villagers of the fire's own town decide whether to fight it, by how big the blaze is, how many already fight it, its importance and how far they are from home | partial 60% | `villager_fire::ReactToFire` has the score, but the group's burning radius, priority and firemen count are taken without tracing them (approximate in [Joining and choosing a fire](../../../world/fire.md#joining-and-choosing-a-fire)), and no test |
| Villagers on their way to worship don't join in | partial 90% | `villager_fire::ReactToFire` tests `WorshipVillager::onWay`, set by `VillagerWorship.cpp`; no test or check covers it |
| Fire fighters beat the flames from 2 m, cooling it by 8 degrees a blow, and are immune to its heat | partial 80% | `villager_fire::PutOutFireByBeating` (within 2 m beyond the larger of the safe and object radius, a burn of -8); firemen are immune in `HeatTransfer`; no test or check covers it. A beating with no fire object goes to DecideWhatToDo instead of keeping its state ([fire.md](../../../world/fire.md#pending)) |
| Villagers never fetch water to fight a fire; those states exist but are never chosen | partial 90% | `villager_fire::PutOutFireWithWater` gives up at once (rows 217, 218); no test or check covers it |
| Villagers that won't fight go round the fire on the side they lean to | done | `villager_fire::MoveAroundFire` with `Fire/ViaPoint`; tests `ViaPoint.MatchesTheOriginalGame` (oracle cases), `ViaPoint.AVillagerGoesRoundAFireInItsWayOnTheSideItLeansTo`, `WayRound.AVillagerGoesRoundEveryFireOfTheGroup` |
| A villager the heat reaches runs: a burning one 4–10 m in random directions while it burns, then goes back to what it was doing | partial 90% | `villager_fire::SetupOnFire`, `OnFire`; no test or check covers it |
| A burning villager dies when its life runs out, credited to whoever made the fire | partial 90% | `fire::traits::DestroyedByEffect` to `villager::DestroyedByEffect` with the fire's player; no test or check covers it on the fire's path |
| A villager leaving a fire reaction always ends up in a sensible state | partial 80% | `villager_reactions::ReactionValidate` and `villager_fire::ShutDownReaction`; the PopFromPrevious tests in `test/test_villager_reactions.cpp` use other reactions, none the fire's. `Living::ProcessReaction` is ported for the slot reactions only (`villager::ProcessReaction`, `src/ECS/Villager/VillagerCore.cpp`); the fire reaction still ends through its own states |
| The town counts injured villagers as fire hurts them | inferred | Not in our wiki's reading of the exe (the burn only feeds `Town::UpdateAggressor`); no count of injured villagers in our tree |
| Animals react to fire and flee it | pending | Animals do not take the fire reaction yet ([animals.md](../../../beings/animal-life.md)) |
| Burning animals die (fall and lie) rather than vanish | partial 90% | `fire::traits::DestroyedByEffect` to `animal_ai::DestroyedByEffect` (dying); no test or check covers it |

## The creature and fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can catch fire; it burns very slowly (it takes about 500 turns of full heat to lose all its life) and faints rather than dies | partial 60% | A creature gets a fire record and `fire::traits::ReduceLifeDueToBurning` takes its body's life; with an opponent it does not yet lose its fight health, and no test |
| A creature that catches gets burn marks on its skin, not flames | pending | No flames on a creature (`src/ECS/Fire/FireEffect.cpp` TODO). The original gives a burning creature three flames at random points of its box ([fire.md](../../../world/fire.md#what-each-class-does-with-fire)); the burn marks are not in our wiki |
| A creature reacts to fires near it, can put fires out, set things on fire, and stop itself burning (with the water miracle if it knows it) | pending | Only its mind's beliefs see fire (`CreatureMindLearning.cpp`); the reaction row 10, the burning creature's water and the town-fire initiative are in [The creature and a fire](../../../world/fire.md#the-creature-and-a-fire); see [../creature/](../creature/) |
| A creature's warmth comes from the climate only: fires don't warm it | partial 90% | `CreaturePhysiologySystem.cpp` reads the weather's temperature; no test or check covers it; see [../creature/desires.md](../creature/desires.md) |

## Buildings, trees and fields burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A burning building empties: its people come out | partial 90% | `abodes::ReduceLife` (`src/ECS/Abodes.cpp`): every inhabitant reacts as to a tap, the building stops working; no test or check covers it on the fire's path |
| A building burnt to nothing flickers out as a ghost and goes; buildings a script holds stay as a building site | partial 50% | `abodes::ReduceLife` makes the building site; `fire::traits::DestroyedByEffect` does nothing for an abode yet (TODO, though `abodes::DestroyedByEffect` exists), so no ghost. See [../building/](../building/) |
| A burning tree darkens, its leaves thin as it heats, and below 0.2 life it narrows (but keeps its height) until it is gone | partial 85% | Darkening capped by the tree brightness (`graphic::TreeDrawColour`), the leaves thinned by the forced ALPHAREF 180..254 (`graphic::TreeAlphaRef`, test `FireGraphic.BurningTreeAlphaRefMatchesTheExe`) and the narrowing to 5 x life below 0.2 (`RenderingSystem.cpp`); the tint and the narrowing have no test, and the ALPHAREF is not yet seen in a capture |
| A tree burnt away is simply removed: there is no burnt-tree stump left | partial 80% | `fire::traits::DestroyedByEffect` deletes it; no test or check covers it. A burning tree that falls should become a burning dead tree (`MoveFire` has no caller yet, [fire.md](../../../world/fire.md#pending)) |
| A burnt field loses its crop and its fire | partial 90% | `fire::traits::DestroyedByEffect` (crops, growth and food to 0, the fire deleted); no test or check covers it |
| A big burning field lights the land around it, as every multi-cell fixed object over 2 m does | partial 90% | A field is an abode, so a multi-cell fixed object ([The light map](../../../world/fire.md#the-light-map)): `graphic::Create` gives it the glow through `map_cells::IsMultiCellStaticClass`; no test or check covers it. (The row said it never lights the land; corrected 2026-10-21) |
| Things that catch lose the reactions they gave (a pot's food, a dead tree's wood) and get them back when the fire ends | partial 70% | `fire::traits::StartOnFire` and `EndOnFire` (`FireObjectTraits.cpp`): dead trees and magic trees get theirs back; a pot does not (TODO); no test |

## Look and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flames: 2 on trees, 7 on things 3 m or bigger, else 2; more grow in as the fire strengthens; each lives 4.3 s, fading in over 1 s | partial 90% | `src/ECS/Fire/FireGraphic.cpp` (most flames, 4.3 s life, 1 s fade-in); no test or check covers it |
| Flames sit at random points on the model (on trees in the lower half; on moving things on their bones) | partial 60% | A random triangle of the mesh, trees in the lower half (`LocalRandomFlamePosition`); moving things' posed bones and the damaged building model are not used; no test |
| Flame size: 0.2 of the height on trees, 0.3 on the creature and the temple, 0.5 on everything else | partial 70% | Trees 0.2 and the rest 0.5 (`src/ECS/Fire/FireGraphic.cpp`); the 0.3 of impressive objects and citadel parts is not ported; no test |
| A fire long out of sight catches up its flames when seen again | partial 60% | The original updates a fire's graphic only when drawn, with a catch-up after 10 turns; our tree updates every fire every frame (approximate, #66) |
| Burning buildings and objects darken as they char and glow red-orange with heat | partial 85% | `graphic::CharringGrey` (test `FireGraphic.CharringGreyMatchesTheExe`, all 256 values), `CharringGlow`; the glow's noise is two sines, not VLNoise (#66) |
| Big burning objects light the land around them with a flickering glow | partial 80% | `src/ECS/Fire/FireGraphic.cpp` (the `S_LMFireBall` light map on multi-cell fixed objects over 2 m); its noise is two sines without the turn fraction, not VLNoise (#66); no test |
| Smoke and steam puffs drift with the smoothed wind and rise | partial 80% | `src/ECS/Fire/FireGraphic.cpp` (rise, growth and the wind pull); drawn centred, while the original's borrow the last flame's origin (approximate); test `FireGraphic.SteamAndSmokeDriftWithHalfTheWindAndRise` (the step: rise, growth, fade and the wind pull) |
| The two burning fires nearest the camera with a strength above a tenth each play a looping crackle | partial 80% | `src/ECS/Fire/FireSound.cpp` (two slots, the nearest fires above 0.1); where the original stops a slot's sound when the same fire takes it, ours keeps it playing (approximate); no test. (The row said one loop; corrected 2026-10-21) |
| The crackle stops when its fire weakens below a tenth | partial 90% | `sound::Consider` (`src/ECS/Fire/FireSound.cpp`); no test or check covers it |
| There is no catching, going-out or collapse sound | done | None in our tree; the original has only the crackle and the steam's sizzle ([fire.md](../../../world/fire.md#the-fires-sound)) |
| Fires are drawn newest first, each its flames (newest first), then its steam, then its smoke | partial 50% | `CollectFires` (`src/ECS/Fire/FireGraphic.cpp`) keeps each fire's order, but walks the fires in a hash map's order, not newest first (#66); no test |
| Flames are orange 0xFF713C (mode 13), steam additive white, smoke grey 0x707070 (mode 6), on the fire and sprite-sheet cells | partial 80% | `src/ECS/Fire/FireGraphic.cpp` (the materials, colours and `frame_anim::FireCell` / `SteamCell`); test `FireGraphic.FireballSteamsInsteadOfFlames` covers only the fireball's steam; not compared with a capture (#66) |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fires (temperature, charring, blaze, fighters) are saved and loaded with the game | pending | openblack has no saved games |
