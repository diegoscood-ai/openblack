# Species and choosing a creature

The player's creature is one of the game's animal species. At the start of the first land the player chooses between
three young creatures; later, silver scroll challenges offer other species ([../story/silver_scrolls/creature_swaps.md](../story/silver_scrolls/creature_swaps.md)), which the player can swap to, keeping
everything their creature has learnt. Each species looks, moves, sounds and grows its own way and has its own
appetite, strength and knack for learning miracles.

**Progress: 32/49 done, 0 chosen, 9 partial, 3 inferred — 73%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

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
| Each species' row in the game's creature tables, the ape's first | done | `creature::InfoRow` (`src/3D/CreatureBody.h`), `creature_mind_body::SpeciesFromRow`; test `SpeciesRowsStartWithTheGiantApe` |

## How species differ

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' size, fatness and strength | done | `CreatureArchetype::StartScale`, `StartBody` from the species tables (the start scale as the `CreaturePhysical` constructor gives it, [body](../../../creature/body.md#growth-size-and-age-as-time-passes)); tests in `test/creature/test_creature_archetype.cpp` |
| Each species walks, runs and goes slowly at its own speeds | done | species speeds from the species' row of the creature tables (`src/InfoConstants.h`), used by `CreatureLocomotionSystem`; see [locomotion.md](locomotion.md) |
| Each species gets hungry, thirsty, tired and grows at its own rates, and likes its own temperature | done | the species' rates (`src/InfoConstants.h`), used by `CreaturePhysiologySystem`; see [physiology.md](physiology.md) |
| Each species has its own innate leanings: how nice, aggressive, lethargic, friendly or talkative it starts | partial 50% | the starting desire tables are read per species ([desires.md](desires.md)); how far the leanings then shape what it does is measured there |
| Some species learn a miracle from fewer sightings than others | inferred | the original multiplies the times to see a miracle by a field of the species' creature info (0x4F8CA0, CreatureInfo +0x37C, [casting](../../../creature/casting.md)); openblack uses raffclar's table of 17 factors (`creature_mind_tables::MiracleMultiplier`), not checked against that field; see [learning_by_observation.md](learning_by_observation.md) |
| Each species pays for miracles with its own energy rates | done | `chantsPerEnergy` and `spellEnergyFloor` from the species' row pay for its miracles (`creature_spell_casting`, through `src/Magic/Core/SpellCreator.cpp`); see [creature_casting.md](creature_casting.md) |
| Each species has its own special fight move | partial 30% | the special order plays and lands at twice the force; how the player calls it is not read; see [fighting.md](fighting.md) |
| Each species leaves its own footprints | done | `src/Creature/CreatureFootprints` per-species prints, drawn by `FootprintSystem`; test `CreatureFootprints.SpeciesCells` |
| Each species has its own places for tattoos | done | see [creature_tattoos.md](creature_tattoos.md) |
| Some species have hair that moves | done | `CreatureHairSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| The ogre can never be held by the hand | done | `creature_hand::MayHold` (`src/Creature/CreatureHandRules.cpp`), as `Creature::ValidForLockedSelectProcess` 0x476E10 refuses species 13 (info +0x1F4); test `CreatureHand.TheHandHoldsAnyPlayersCreature` |
| Other ogre limits (unconfirmed which) | inferred | a guess: no other ogre test is read in the exe |
| The hand's tooltip and the creature's help texts name its species | inferred | whether the original's tooltip or help texts on a creature name its species is not read; the hand's tooltip on a creature (`HandToolTips.cpp`) does not |

## Choosing the first creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the first land the player is shown three young creatures to choose from: an ape, a cow and a tiger | partial 50% | the tutorial (`CreaturesInGlade`) makes the cow, the ape and the tiger with `CREATE` type 12 and gives one with `CREATURE_SET_PLAYER` ([the tutorial's creatures](../../../creature/players-creature.md#the-tutorials-creatures)); both are real (`src/CHLApi.cpp`; test `ScriptLoadCreatureTest.AScriptsCreateMakesANewCreatureOfNobodyOutOfReactions`); not checked in play; the shipped quest: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md) |
| Each of the three is shown off, and the good and evil advisors argue for and against it | pending |  |
| Two clicks on the same creature choose it; the other two are deleted | pending | the shipped quest's rule: [../story/gold_scrolls/choose_your_creature.md](../story/gold_scrolls/choose_your_creature.md#choosing) |
| The chosen creature takes the name the player gave in their profile | partial 20% | the player page holds a creature name (`src/Gui/GameMenu.cpp`) and `SET_CREATURE_NAME` names a creature (`player_creature::SetName`); `CREATURE_SET_PLAYER` does not give it the player's name as the original does ([the tutorial's creatures](../../../creature/players-creature.md#the-tutorials-creatures)) |
| A player who already has a creature from an earlier game can keep it and skip the creature training | partial 50% | `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE` work with the `--creature-file` setting (`ECS/PlayerCreature.cpp`), `IS_KEEPING_OLD_CREATURE` and `CAN_SKIP_CREATURE_TRAINING` read the land's skip flags (`src/CHLApi.cpp`); no profile choice and no saving back |
| The new creature becomes the player's leashable creature | done | `LeashSystem::ClaimOnArrival`, called from `CreatureArchetype::Create`; tests `test_leash_ownership.cpp` |
| The new creature starts young, at the first stage of growing up | done | the tutorial's creatures are made at the first stage with every desire off and no stage moved (`player_creature::StartAtFirstStage` from `MakeForScript`), as the original's ([development phases](../../../creature/players-creature.md#the-development-phases)); the chosen one's growing up starts with the script's `DEV_FUNCTION` 1 and `SET_CREATURE_DEV_STAGE` moves it as the original; test `ScriptLoadCreatureTest.AScriptsCreateMakesANewCreatureOfNobodyOutOfReactions`; see [development_phases.md](development_phases.md) |

## Swapping species later

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll challenges in the lands offer other species: the breeder, and the swaps to the ape, the cow and the brown bear | partial 50% | the challenge scripts (`CreatureBreeder`, `SwapToApe`, `SwapToCow`, `SwapToBrownBear`) make the creatures with `CREATE` type 12 and swap with `SWAP_CREATURE`, both real; not checked in play; see [../story](../story/) |
| The offered creature waits for the player to come and look at it | pending |  |
| Choosing it swaps the player's creature for the new species, keeping its mind: desires, learning, opinions, known actions and miracles | done | `SWAP_CREATURE`, `CreatureMindSystem::SwapMinds`, as `Creature::SwapMinds` 0x47BEE0 ([swapping minds](../../../creature/players-creature.md#swapping-minds)); tests `test/creature/test_creature_swap.cpp`; see [creature_swaps.md](../story/silver_scrolls/creature_swaps.md) |
| Each body keeps its own name, size and alignment, then grows into the other's size and alignment; the strength moves with the mind | done | as the exe (0x47BF49..0x47C24F, [swapping minds](../../../creature/players-creature.md#swapping-minds)); tests `CreatureSwap.EachTurnClosesAShareOfTheGap`, `SettledWhenBothGapsAreUnderAHundredth`, `SightingsKeepTheirShareInTheNewSpecies` |
| Every bonus creature (the leopard, horse, mandrill, gorilla and rhino) is on offer: the exe unlocks them all | done | `creature_swap::k_GameUnlocks`, passed by `IS_CREATURE_AVAILABLE`, as the frozen exe's unlock test returns 1 ([the bonus creatures' unlock](../../../creature/players-creature.md#the-bonus-creatures-unlock)); test `CreatureSwap.TheGameHasEveryBonusCreatureUnlocked` |
| The old creature leaves | pending |  |
| Scripts can create a creature next to another | partial 70% | `CREATURE_CREATE_RELATIVE_TO_CREATURE` (`CreatureCreateRelativeToCreature`, `src/CHLApi.cpp`): the owner, reactions off, size and strength from the exe ([a creature made from another](../../../creature/players-creature.md#a-creature-made-from-another)); the camera-look switch off for it and the weak-strong target are not ported, and no test covers it |
| Scripts can make a creature, give it to a player and name it | done | `CREATE` makes a creature of no player (`player_creature::MakeForScript`), `CREATURE_SET_PLAYER` makes it the player's (`MakePlayersCreature`) and `SET_CREATURE_NAME` names it (`player_creature::SetName`), as the exe's create switch ([the tutorial's creatures](../../../creature/players-creature.md#the-tutorials-creatures)); tests `ScriptLoadCreatureTest.AScriptsCreateMakesANewCreatureOfNobodyOutOfReactions`, `CreatureMindSystemTest` (the name) |

## Other gods' creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from their own mind files | partial 50% | land scripts can create a creature from a mind file (`FeatureScriptCommands::CreateCreatureFromFile`); the challenge scripts' `LOAD_CREATURE` is real, so the story's creatures are made; not checked in play |
| Skirmish and computer players' creatures come from the game's template minds (destroy towns, impress towns, protect towns, destroy other creatures) | partial 20% | the files can be read (`components/creaturemind`); computer players aren't in openblack ([../multiplayer](../multiplayer/)) |
| Scripts can set what a computer player's creature likes | pending | no such native is handled; the computer player's hand natives in `src/CHLApi.cpp` are real; not checked in play |
