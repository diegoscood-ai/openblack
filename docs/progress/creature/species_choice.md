# Species and choosing a creature

The player's creature is one of the game's animal species. At the start of the first land the player chooses between
three young creatures; later, silver scroll challenges offer other species ([../story/silver_scrolls/creature_swaps.md](../story/silver_scrolls/creature_swaps.md)), which the player can swap to, keeping
everything their creature has learnt. Each species looks, moves, sounds and grows its own way and has its own
appetite, strength and knack for learning miracles.

**Progress: 26/48 done, 9 partial — 64%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## The species

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ape (the giant ape), with its own body, animations and voice | done | `CreatureType::GiantApe`; made with its rig, meshes, morphs and voice bank by `CreatureArchetype::Create` (`src/ECS/Archetypes/CreatureArchetype.cpp`, rigs from `Data/CTR` through `src/Resources/Loaders.cpp`), from `LOAD_MY_CREATURE`, `CREATE_CREATURE` or the debug spawner; tests `CreatureArchetype*`, `CreatureRigLoader*` |
| Cow | done | as above |
| Tiger | done | as above |
| Leopard | done | as above |
| Wolf | done | as above |
| Lion | done | as above |
| Horse | done | as above |
| Tortoise | done | as above |
| Zebra | done | as above; offered by [The Riddles](../story/silver_scrolls/the_riddles.md) |
| Brown bear | done | as above |
| Polar bear | done | as above |
| Sheep | done | as above |
| Chimp | done | as above |
| Ogre | done | as above; its bank is the "greek" one (`src/Creature/CreatureAudio.cpp`; test `CreatureBody.TheOgresVariantsKeepItsBaseName`) |
| Mandrill | done | as above |
| Rhino | done | as above |
| Gorilla | done | as above |
| Chicken and crocodile, listed by the game but with no bodies in its data | n/a | never playable in the five lands |
| Species added by Creature Isle | n/a | Creature Isle |
| Each species' row in the game's creature tables, the ape's first | done | `creature::InfoRow` (`src/3D/CreatureBody.h`), `creature_mind_body::SpeciesFromRow`; test `SpeciesRowsStartWithTheGiantApe` |

## How species differ

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' size, fatness and strength | done | `CreatureArchetype::StartScale`, `StartBody` from the species tables |
| Each species walks, runs and goes slowly at its own speeds | done | species speeds from the species' row of the creature tables (`src/InfoConstants.h`), used by `CreatureLocomotionSystem`; see [locomotion.md](locomotion.md) |
| Each species gets hungry, thirsty, tired and grows at its own rates, and likes its own temperature | done | the species' rates (`src/InfoConstants.h`), used by `CreaturePhysiologySystem`; see [physiology.md](physiology.md) |
| Each species has its own innate leanings: how nice, aggressive, lethargic, friendly or talkative it starts | partial | the starting desire tables are read per species ([desires.md](desires.md)) |
| Some species learn a miracle from fewer sightings than others | partial | `src/Creature/CreatureWatching` per-species miracle rules (`creature_mind_tables::MiracleMultiplier`), reached only from the debug spawner's `SeeMiracle`; see [learning_by_observation.md](learning_by_observation.md) |
| Each species pays for miracles with its own energy rates | partial | `chantsPerEnergy`, `spellEnergyFloor` are read in the species' row (`src/InfoConstants.h`), but the creature casts no miracle; see [creature_casting.md](creature_casting.md) |
| Each species has its own special fight move | partial | see [fighting.md](fighting.md) |
| Each species leaves its own footprints | done | `src/Creature/CreatureFootprints` per-species prints, drawn by `FootprintSystem`; test `CreatureFootprints.SpeciesCells` |
| Each species has its own places for tattoos | done | see [creature_tattoos.md](creature_tattoos.md) |
| Some species have hair that moves | done | `CreatureHairSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| The ogre can never be held by the hand | done | `creature_hand::MayHold` (`src/Creature/CreatureHandRules.cpp`); test `CreatureHand.ACreatureOfAnyPlayerLetsTheHandHoldIt` |
| Other ogre limits (unconfirmed which) | todo |  |
| The hand's tooltip and the creature's help texts name its species | todo | the hand's tooltip on a creature (`HandToolTips.cpp`) does not name the species |

## Choosing the first creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the first land the player is shown three young creatures to choose from: an ape, a cow and a tiger | todo | the tutorial makes them with `CREATE` type 12 and `CREATURE_SET_PLAYER`; our `CreateScriptObject` makes no creature and `CREATURE_SET_PLAYER` is a stub (`src/CHLApi.cpp`); the shipped quest: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md) |
| Each of the three is shown off, and the good and evil advisors argue for and against it | todo |  |
| Picking one up and putting it down, or clicking it, chooses it; the others leave | todo | in the shipped quest the choice is two clicks on the same creature, and the other two are deleted rather than leaving: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md#choosing) |
| The chosen creature takes the name the player gave in their profile | partial | the player page holds a creature name (`src/Gui/GameMenu.cpp`); nothing names the creature from it |
| A player who already has a creature from an earlier game can keep it and skip the creature training | partial | `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE` work with the `--creature-file` setting (`ECS/PlayerCreature.cpp`), `IS_KEEPING_OLD_CREATURE` and `CAN_SKIP_CREATURE_TRAINING` read the land's skip flags (`src/CHLApi.cpp`); no profile choice and no saving back |
| The new creature becomes the player's leashable creature | done | `LeashSystem::ClaimOnArrival`, called from `CreatureArchetype::Create`; tests `test_leash_ownership.cpp` |
| The new creature starts young, at the first stage of growing up | partial | see [development_phases.md](development_phases.md) |

## Swapping species later

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll challenges in the lands offer other species (unconfirmed which in each land) | todo | see [../story](../story/) |
| The offered creature waits for the player to come and look at it | todo |  |
| Choosing it swaps the player's creature for the new species, keeping its mind: desires, learning, opinions, known actions and miracles | todo | `SWAP_CREATURE` is a stub (`src/CHLApi.cpp`) |
| The swap keeps the creature's name, alignment, strength and size (unconfirmed which carry over) | todo |  |
| The old creature leaves | todo |  |
| Scripts can create a creature next to another | todo | `CREATURE_CREATE_RELATIVE_TO_CREATURE` is a stub |
| Scripts can make a creature, give it to a player and name it | todo | `CREATE` makes no creature; `CREATURE_SET_PLAYER`, `SET_CREATURE_NAME` are stubs |

## Other gods' creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from their own mind files | partial | land scripts can create a creature from a mind file (`FeatureScriptCommands::CreateCreatureFromFile`); the challenge scripts' `LOAD_CREATURE` is a stub, so the story's creatures aren't created |
| Skirmish and computer players' creatures come from the game's template minds (destroy towns, impress towns, protect towns, destroy other creatures) | partial | the files can be read (`components/creaturemind`); computer players aren't in openblack ([../multiplayer](../multiplayer/)) |
| Scripts can set what a computer player's creature likes | todo | no such native is handled; the computer player natives in `src/CHLApi.cpp` are stubs |
