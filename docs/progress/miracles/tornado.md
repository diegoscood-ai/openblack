# Tornado

The tornado is the storm's second power-up: a storm whose centre grows a spinning funnel that wanders over the land,
sucks up trees, people, animals and loose objects, carries them round and flings them away. The storm's clouds, rain
and in-cloud lightning are described in [storm.md](storm.md).

**Progress: 17/37 done, 0 chosen, 13 partial, 5 inferred — 74%**

How the original does it, in our wiki: [Vortexes and the tornado: objects swallowed, carried and flung](../../../world/vortex.md), [Miracles one by one](../../../magic/storm-and-tornado.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tornado comes with the storm's clouds but no rain; its in-cloud lightning does not set fires | done | `src/Particles/Rules/Storm.cpp`, `src/Magic/Spells/SpellStormAndTornado.cpp` (the info.dat rows: the tornado's rain amount is 0, [page](../../../magic/storm-and-tornado.md#the-spell)); scenario `miracles.tornado_village` |
| Upkeep each turn grows with the square of the radius, and each object picked up costs prayer power | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (CostToMaintain), `src/Particles/Rules/Storm.cpp` (each pick-up asks the spell, which pays for the event) Test `Storm.radiusClampAndUpkeep` |
| The funnel's foot pushes and hurts what it passes over a little, every step | partial 80% | `src/Particles/Rules/Storm.cpp` (an event at the base every step while not closing) No test or check covers the base's event |

## The funnel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The funnel fades in over its fade-in time and fades out after the storm ends | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado) Scenario `miracles.tornado_village` |
| The foot wanders on a smooth noise path; the top follows the storm once the delay before moving has passed | done | `src/Particles/Rules/Storm.cpp`, `src/Particles/Noise.cpp`; test `Storm.tornadoFunnel` (`test/test_storm.cpp`) |
| The foot keeps to the land's height, the top stands a size-scaled height above it | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado) Scenario `miracles.tornado_village` |
| The funnel bends between foot and top and wiggles with height, the wiggle following the bend | done | `src/Particles/Rules/Storm.cpp` (`storm::ThetaDot` and `storm::FunnelCentre`: the bias and gain bend with the processor's own logarithm and power, the wiggle's phase from the gain with its own cosine and sine); tests `TornadoBits.TheFunnelsBendGivesTheOriginalsBits`, `TornadoBits.TheFunnelsCentreLineGivesTheOriginalsBits` (`test/test_storm_x87.cpp`), `Storm.tornadoFunnel` |
| Two translucent spinning shells, tinted by the land, make up the funnel | done | `src/Particles/Creators/Mesh.cpp` (two meshes drawn with the land's colour); checked by day per our wiki |
| Dust is thrown up from the ground at the foot, coloured by the land's material, snow-coloured on snowy ground, at most 50 pieces | partial 90% | `src/Particles/Rules/Storm.cpp` (TornadoDustColour from the land's material, at most 50) Scenario `miracles.tornado_village`. openblack checks the island side of the dust cell, the original passes the cell on unchecked ([page](../../../magic/storm-and-tornado.md#the-tornado)) |
| Make-believe bushes and chickens are tossed up only over dry land | done | `src/Particles/Rules/Storm.cpp` (EmitPretendObjects, only on dry land) Scenario `miracles.tornado_village` |
| A roaring loop starts with the funnel and fades out softly when it ends | partial 80% | The effect's sound, `src/Audio/Services/SpellSounds.cpp`. No test or check covers the loop's start and fade |
| The tornado's storm is turned aside by shields it passes over | partial 80% | `src/Particles/Rules/Storm.cpp` (UR_CloudMoverNew bounces the cores off shields at the tornado's level), `src/Particles/Rules/Shield.cpp`; no test or check covers it |

## Sucking things up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing is picked up until the funnel has faded in; then at most one thing every three turns | done | `src/Particles/Rules/Storm.cpp` (PickUp: once every third turn after the fade-in) Test `Storm.theTornadosPickUpOrderAndReach` |
| The reach grows with the funnel's size and with tribal power, up to five times | partial 90% | The formula is in `src/Particles/Rules/Storm.cpp` (test `Storm.theTornadosPickUpOrderAndReach`), and `PlayerMagic::tribalPower` is 1 for every tribe until a wonder of that tribe is built for the caster's player, and rises by each wonder's power (`wonders::AddToPlayer`, copied each turn by `magic::players::ProcessTribalPowers`, as `Wonder::AddToPlayer` 0x778FC0 and `GPlayer::Process`); not yet seen in a run with a built wonder; see [prayer_cost.md](prayer_cost.md) |
| The search goes outward cell by cell from the foot, fixed things before moving things, and the first thing taken ends it | done | `src/Particles/Rules/Storm.cpp` (a cell spiral, each object in its own cell, the fixed list first) Test `Storm.theTornadosPickUpOrderAndReach` |
| Trees, villagers (dead ones too, but not those hidden in buildings), animals, rocks and other loose things, and small pots can be lifted; buildings never | done | `src/Particles/Rules/Storm.cpp` (CanSuckUp), `src/ECS/Physics/ParticleCarriedObjects.cpp`; a dying villager is not carried. Scenarios `miracles.tornado_village`, `miracles.tornado_animals` |
| Only things small enough for the funnel are lifted | partial 80% | `src/Particles/Rules/Storm.cpp` (CanSuckUp: twice the base radius and the next radius larger than the object) No test or check covers the size test |
| A food or wood pile too big to lift gives up a pot of 150 to 650 by the funnel's size; a pile dropped from the hand is lifted whole | partial 70% | `src/Particles/Rules/Storm.cpp` (SplitPile: a new hand pile of 150 to 650 by the funnel's size) Scenario `miracles.tornado_hand_pile`. A pile left loose gives up its amount and sinks as the hand's does, not as the original; the emptied pile is not deleted; piles are searched outside the map grid ([wiki](../../../magic/storm-and-tornado.md#openblack); issue #55) |
| A pile at a storage pit gives its share from the pit's store | partial 80% | `src/Particles/Rules/Storm.cpp` (SplitPile takes a store's pile from its store, as the hand does), as the original ([page](../../../magic/storm-and-tornado.md#the-tornado)); no test or check covers it |
| Only things the miracle can destroy are taken | partial 80% | `src/Particles/Rules/Storm.cpp` (the can-destroy event), `src/Particles/Rules/Explosion.cpp` (CanBeDestroyedBySpell) No test or check covers it |
| Field crops are lifted too | inferred | Not in our wiki's reading of the pick-up (any available object that fits the funnel is lifted): whether a field's crops are one is not checked; openblack has no field-crop objects |
| Things a script marks as immovable stay put | inferred | The original's take-up test skips objects with one flag bit set whose meaning is still open ([wiki Pending](../../../magic/storm-and-tornado.md#pending)); the flag is not checked in `CanSuckUp` (`src/Particles/Rules/Storm.cpp`) |

## Carrying and flinging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is caught spirals up around the funnel, pulled round the bend and wiggle | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado flying atoms; each step's turn round the funnel by `storm::OrbitPoint` with the processor's own arctangent, cosine and sine); tests `TornadoBits.AFlyingAtomsNextPointGivesTheOriginalsBits`, `TornadoBits.TheFlyingAtomsSpinGivesTheOriginalsBits` (`test/test_storm_x87.cpp`) |
| Things near the top are flung out; anything still carried is let go 15 s after it was caught, wherever it is | done | `src/Particles/Rules/Storm.cpp` (the flying atoms go to the falling group above 0.9 of the height or after 15 s) Scenario `miracles.tornado_village` |
| Carried things follow their place on the funnel smoothly between turns | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier), `src/ECS/Physics/ParticleCarriedObjects.cpp`. Scenario `miracles.tornado_village` |
| Villagers let go die where they land, killed by the caster's miracle | partial 80% | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release: `villager::DestroyedByEffect` with the caster) Scenario `miracles.tornado_village`. The villager dies where its particle was last seen (at the turn or the frame), not proven to be the original's point ([wiki](../../../magic/storm-and-tornado.md#openblack)) |
| Trees and pots let go vanish | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release) Scenario `miracles.tornado_village` |
| Something no longer there when let go is simply dropped | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release: an object no longer available is only let go) Test `TornadoCreature.AThingNoLongerThereIsNotCaught` |
| Animals let go play their dying clip, lie dead and are removed later | partial 70% | `src/Particles/Rules/Storm.cpp` to `animal_ai::Kill` (the species' dying clip, the corpse, then a smoke puff, `src/ECS/AnimalAI.cpp`); not checked against the game for every species, nor birds falling. See [../animal/](../animal/) |
| When the storm ends, what is still carried is flung out | partial 80% | `src/Particles/Rules/Storm.cpp` (on closing the carried atoms go to the falling group) No test or check covers it |
| A thing newly caught joins the carried ones on the next step | pending | The original links the new collection at the head of its list, so it enters on the next step; openblack updates it in the step it is made ([wiki](../../../magic/storm-and-tornado.md#openblack); issue #55) |
| A carried villager or animal is held in flight as the original holds it | inferred | openblack moves it out of physics into the held-in-hand state (`animal_ai::PlaceInHand`); what the original's physics start does with it is not read ([wiki Pending](../../../magic/storm-and-tornado.md#pending)) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature in reach is never lifted; it faints where it is, the leash lets go, and it is not caught again while out cold | done | `TornadoSystem::CatchCreature` (`src/ECS/Systems/Implementations/TornadoSystem.cpp`) through the fight system's `ForceFaint`, as the original's faint ([page](../../../magic/storm-and-tornado.md#the-tornado)); tests `TornadoCreature.ACreatureCaughtFaintsHelplessAndNotTwiceWhileOutCold`, `TornadoCreature.OnceRecoveredItCanBeCaughtAgain`; scenarios `miracles.tornado_creature`, `miracles.tornado_straight_at_creature` |
| After the faint it carries on from where it lay, waiting for spells on it to end | inferred | It comes round as from a knock-out (the fight system); that the original waits for spells on it to end is not in our wiki and not checked |
| The player is told by the creature help that the creature has fainted | inferred | Not in our wiki's reading of the tornado's faint; not checked against the original |
| A creature can cast the tornado as an attacking miracle | partial 60% | The creature's plan has a tornado cast (`CastTornado`, `src/Creature/CreaturePlanActions.cpp`); no scenario or test casts it; not yet checked against the original (issue #172) |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A running tornado and what it carries are kept in a saved game | pending | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
