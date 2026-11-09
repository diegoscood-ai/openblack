# Wild animals

The animals that live wild on the lands: hunters (lions, tigers, leopards, wolves) and grazers that aren't kept by towns
(zebras, tortoises, goats in the wild). Shared behaviour is in [animal behaviour](animal_behaviour.md).

**Progress: 6/18 done, 0 chosen, 12 partial, 0 inferred — 90%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../../beings/animal-life.md), [Skeletal animation (villagers and animals)](../../../beings/animal-life.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place wild animals, alone or in flocks | done | CREATE_ANIMAL, CREATE_NEW_ANIMAL and CREATE_FLOCK (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AnimalArchetype.cpp`); the same code as the birds' (testbed `birds.every_kind`) |
| Each kind has its own model, size by age, speeds and table values (sight, hunting, domain, flock size) | done | the animal info table (`src/InfoConstants.h`), read by the AI, the mesh and the scale for age (`src/ECS/AnimalAI.cpp`); `AnimalSystemTest.ScaleAndRadius`, `AnimalAgeTest.AYoungAnimalGrowsUp` |
| Each kind plays its own clips: stand, move, eat, start and finish eating, sleep, in hand, thrown, landed, dying, dead, and for hunters stalk, pounce and hide | partial 80% | the clips per state and species, the hunters' stalk, pounce and lair clips included (`src/ECS/AnimalAnimations.cpp`); `test/test_living_first_clip.cpp`. Missing: the moving clip plays by the clock, not by the ground; an animal in a dance plays its move clip; a held one-shot clip's sound events are not replayed; the first clip is chosen in the frame after the animal is made (issue #12) ([animal life, Pending](../../../beings/animal-life.md#pending)) |
| Animals live in a domain about where they were made, and keep to it | done | the flock's domain and the leader's moves within it (`components::Flock`, `src/ECS/AnimalAI.cpp`); `FlocksTest.PosWithinDomain`, `FlocksTest.SeparateKeepsTheDomain` |

## Hunters

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hunters grow hungry each turn and hunt once hungry enough | partial 90% | the hunger counter and its threshold per kind (`src/ECS/AnimalPredators.cpp`); no test pins it |
| A hunter looks for prey in the cells spiralling out from its own, remembering where it last found some | partial 90% | `FindPrey` in a spiral of cells, the target seen last turn chased first (`src/ECS/AnimalPredators.cpp`); no test pins it |
| It chases its prey, and leaps at it from close enough while facing it | partial 75% | the chase, the stalk and sprint speeds, the pounce within its stride and angle (`src/ECS/AnimalPredators.cpp`); testbed `animals.predators_village`. Missing: the step towards the prey has no arrival test and no early return (`StepTowards`), and the hunt tests the prey before stepping where the original steps first (audit D14) ([animals, Pending](../../../beings/animals.md#pending)) |
| Prey brought down falls, is eaten over the turns, then dies | done | `DOWNED`, `BEING_EATEN` for 300 turns, then dead (`src/ECS/AnimalPredators.cpp`, `src/ECS/Life.cpp`); testbed `animals.predators_village` |
| A hunter that can't reach its prey (a villager indoors) gives it up | partial 90% | the prey test asks whether a villager is reachable (`villager::IsReachable`: available, not at home, top state not 236), and a target no longer available is dropped at the turn's start (`src/ECS/AnimalPredators.cpp`); no test pins it for a hunter |
| A hunter stalks and hides before it pounces | partial 85% | the stalk from 100 to 50 m and the wolves' hiding in their lair (`src/ECS/AnimalPredators.cpp`); the flock's calm, which the wolf's needs and its hiding read in the original, is written but never read (audit D8) |
| Hunters have a lair near the forests, where they hide and sleep | partial 85% | the tiger's and wolf's lairs from the forests and big forests, the lion and leopard where they are (`src/ECS/AnimalLairs.cpp`); a lone hunter's lair is computed at its first turn, where the original computes it as it is made (audit D12), and the calm is not read (D8) |
| Lions frighten creatures | done | lions, tigers, leopards and wolves are among what frightens a creature (`AnimalSystem::IsFrighteningToCreature`); `AnimalSystemTest.BatsVulturesBigCatsAndWolvesFrightenCreatures`; [mind](../../../creature/mind.md#what-frightens-a-creature-and-the-fire-it-believes) |
| Other animals flee from hunters | partial 85% | the flee-from-predator reaction, made when a predator is created (`src/ECS/AnimalFlee.cpp`), its turns before reacting again by the true distance; `AnimalRules.AnAnimalIsFarEnoughFromAPredatorByItsTrueDistance`, `test/test_predator_flee.cpp`. Missing: the reaction switch of audit D1 and the movement direction of D4 (see [animal behaviour](animal_behaviour.md)) |
| Hunters attack villagers and livestock | done | villagers and other animals are prey (`src/ECS/AnimalPredators.cpp`); testbed `animals.predators_village`; see ../villager/death.md |
| Hunters never hunt what cannot be eaten (what came out of a vortex) | partial 90% | `script_held::CannotBeEaten` refuses the prey and ends a hunt (`src/ECS/AnimalPredators.cpp`); no test pins it |

## Grazers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grazers look for good grass, wander there and graze | partial 75% | `LookForFoodPos`, the wander and the eating states (`src/ECS/AnimalAI.cpp`); `AnimalRules.AGrazersFoodSearchGoesRoundATenthOfItsDomainSquaredInFloats`. Missing: no refusal of a cell with a fixed object (audit D6), the wander's three tries (D10) |
| Grazers flock together and follow their leader | partial 90% | the flock steering and the leader (`src/ECS/AnimalAI.cpp`, `components::Flock`); `AnimalSystemTest.TheLeaderIsTheFirstMemberStillThere`; the steering is not pinned |
| Tortoises are the slowest grazers, and take no reactions | partial 90% | a grazer of the shared class with its own table values (0.25 m/s), which takes no reactions (`src/ECS/AnimalAI.cpp`); no test pins it ([animal life](../../../beings/animal-life.md#reactions)) |
