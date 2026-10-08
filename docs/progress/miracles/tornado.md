# Tornado

The tornado is the storm's second power-up: a storm whose centre grows a spinning funnel that wanders over the land,
sucks up trees, people, animals and loose objects, carries them round and flings them away. The storm's clouds, rain
and in-cloud lightning are described in [storm.md](storm.md).

**Progress: 26/34 done, 3 partial — 81%**

How the original does it, in our wiki: [Vortexes and the tornado: objects swallowed, carried and flung](../../bw1-notes/vortex.md), [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tornado comes with the storm's clouds and rain; its in-cloud lightning does not set fires | done | `src/Particles/Rules/Storm.cpp`, `src/Magic/Spells/SpellStormAndTornado.cpp` (the info.dat rows). Our wiki differs: the tornado's row has no rain (rain amount 0), so the tornado does not rain ([page](../../bw1-notes/miracles.md#the-spell-spellstormandtornadocpp-0xf8-bytes-vtable-0x9847dc)) |
| Upkeep each turn grows with the square of the radius, and each object picked up costs prayer power | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (CostToMaintain), `src/Particles/Rules/Storm.cpp` (each pick-up asks the spell, which pays for the event) |
| The funnel's foot pushes and hurts what it passes over a little, every step | done | `src/Particles/Rules/Storm.cpp` (an event at the base every step while not closing) |

## The funnel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The funnel fades in over its fade-in time and fades out after the storm ends | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado) |
| The foot wanders on a smooth noise path; the top follows the storm once the delay before moving has passed | done | `src/Particles/Rules/Storm.cpp`, `src/Particles/Noise.cpp`; test `Storm.tornadoFunnel` (`test/test_storm.cpp`) |
| The foot keeps to the land's height, the top stands a size-scaled height above it | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado) |
| The funnel bends between foot and top and wiggles with height, the wiggle following the bend | done | `src/Particles/Rules/Storm.cpp` (the bias and gain bend and the wiggle); test `Storm.tornadoFunnel` |
| Two translucent spinning shells, tinted by the land, make up the funnel | done | `src/Particles/Creators/Mesh.cpp` (two meshes drawn with the land's colour); checked by day per our wiki |
| Dust is thrown up from the ground at the foot, coloured by the land's material, snow-coloured on snowy ground, at most 50 pieces | done | `src/Particles/Rules/Storm.cpp` (TornadoDustColour from the land's material, at most 50) |
| Make-believe bushes and chickens are tossed up only over dry land | done | `src/Particles/Rules/Storm.cpp` (EmitPretendObjects, only on dry land) |
| A roaring loop starts with the funnel and fades out softly when it ends | done | The effect's sound, `src/Audio/Services/SpellSounds.cpp` |
| The tornado's storm is turned aside by shields it passes over | done | (added) `src/Particles/Rules/Storm.cpp` (UR_CloudMoverNew bounces the cores off shields at the tornado's level), `src/Particles/Rules/Shield.cpp` |

## Sucking things up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing is picked up until the funnel has faded in; then at most one thing every three turns | done | `src/Particles/Rules/Storm.cpp` (PickUp: once every third turn after the fade-in) |
| The reach grows with the funnel's size and with tribal power, up to five times | partial | The formula is in `src/Particles/Rules/Storm.cpp`, but `PlayerMagic::tribalPower` is never set in our tree, so it is always 1 |
| The search goes outward cell by cell from the foot, fixed things before moving things, and the first thing taken ends it | done | `src/Particles/Rules/Storm.cpp` (a cell spiral, each object in its own cell, the fixed list first) |
| Trees, villagers (dead ones too, but not those hidden in buildings), animals, rocks and other loose things, and small pots can be lifted; buildings never | done | `src/Particles/Rules/Storm.cpp` (CanSuckUp), `src/ECS/Physics/ParticleCarriedObjects.cpp`; a dying villager is not carried |
| Only things small enough for the funnel are lifted | done | `src/Particles/Rules/Storm.cpp` (CanSuckUp: twice the base radius and the next radius larger than the object) |
| A food or wood pile too big to lift gives up a pot of 150 to 650 by the funnel's size; a pile dropped from the hand is lifted whole | done | `src/Particles/Rules/Storm.cpp` (SplitPile: a new hand pile of 150 to 650 by the funnel's size) |
| Storage pits are never emptied | done | `src/Particles/Rules/Storm.cpp` (SplitPile takes a store's pile from its store, as the hand does). Our wiki differs: a pile at a store gives its share from the store ([page](../../bw1-notes/miracles.md#the-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680)) |
| Only things the miracle can destroy are taken | done | `src/Particles/Rules/Storm.cpp` (the can-destroy event), `src/Particles/Rules/Explosion.cpp` (CanBeDestroyedBySpell) |
| Field crops are lifted too | todo | openblack has no field-crop objects |
| Things a script marks as immovable stay put | todo | The object flag is not checked (a note in CanSuckUp, `src/Particles/Rules/Storm.cpp`); script object flags are not in openblack |

## Carrying and flinging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is caught spirals up around the funnel, pulled round the bend and wiggle | done | `src/Particles/Rules/Storm.cpp` (UR_Tornado flying atoms) |
| Things near the top are flung out; anything still carried is let go 15 s after it was caught, wherever it is | done | `src/Particles/Rules/Storm.cpp` (the flying atoms go to the falling group above 0.9 of the height or after 15 s) |
| Carried things follow their place on the funnel smoothly between turns | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier), `src/ECS/Physics/ParticleCarriedObjects.cpp` |
| Villagers let go die where they land, killed by the caster's miracle | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release: `villager::DestroyedByEffect` with the caster) |
| Trees and pots let go vanish | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release) |
| Something no longer there when let go is simply dropped | done | `src/Particles/Rules/Storm.cpp` (TornadoCarrier::Release: an object no longer available is only let go) |
| Animals let go play their dying clip, lie dead and are removed later | partial | `src/Particles/Rules/Storm.cpp` to `animal_ai::Kill` (the species' dying clip, the corpse, then a smoke puff, `src/ECS/AnimalAI.cpp`); not checked against the game for every species, nor birds falling. See [../animal/](../animal/) |
| When the storm ends, what is still carried is flung out | done | `src/Particles/Rules/Storm.cpp` (on closing the carried atoms go to the falling group) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature in reach is never lifted; it faints once, the leash lets go, and it lies a while before getting up | partial | A creature can't become a carried object, so it is never lifted; its own handling (the faint and the leash) is not in `src/Particles/Rules/Storm.cpp` |
| After the faint it carries on from where it lay, waiting for spells on it to end | todo | No faint from the tornado in our tree |
| The player is told by the creature help that the creature has fainted | todo | No creature help message for it in our tree |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A running tornado and what it carries are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
