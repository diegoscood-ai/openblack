# Wild animals

The animals that live wild on the lands: hunters (lions, tigers, leopards, wolves) and grazers that aren't kept by towns
(zebras, tortoises, goats in the wild). Shared behaviour is in [animal behaviour](animal_behaviour.md).

**Progress: 16/18 done, 0 partial — 89%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../bw1-notes/animals.md), [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place wild animals, alone or in flocks | done | CREATE_ANIMAL, CREATE_NEW_ANIMAL and CREATE_FLOCK (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AnimalArchetype.cpp`) |
| Each kind has its own model, size by age, speeds and table values (sight, hunting, domain, flock size) | done | the animal info table (`src/InfoConstants.h`), read by the AI, the mesh and the scale for age (`src/ECS/AnimalAI.cpp`) |
| Each kind plays its own clips: stand, move, eat, start and finish eating, sleep, in hand, thrown, landed, dying, dead, and for hunters stalk, pounce and hide | done | the clips per state and species, the hunters' stalk, pounce and lair clips included (`src/ECS/AnimalAnimations.cpp`) |
| Animals live in a domain about where they were made, and keep to it | done | the flock's domain and the leader's moves within it (`components::Flock`, `src/ECS/AnimalAI.cpp`) |
| Animals keep to the surroundings they like (land, coast; unconfirmed) | todo |  |
| The chess puzzle's animals and villagers (Creature Isle) | n/a | Creature Isle only |

## Hunters

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hunters grow hungry each turn and hunt once hungry enough | done | the hunger counter and its threshold per kind (`src/ECS/AnimalPredators.cpp`) |
| A hunter looks for prey in the cells spiralling out from its own, remembering where it last found some | done | `FindPrey` in a spiral of cells, the target seen last turn chased first (`src/ECS/AnimalPredators.cpp`) |
| It chases its prey, and leaps at it from close enough while facing it | done | the chase, the stalk and sprint speeds, the pounce within its stride and angle (`src/ECS/AnimalPredators.cpp`) |
| Prey brought down falls, is eaten over the turns, then dies | done | `DOWNED`, `BEING_EATEN` for 300 turns, then dead (`src/ECS/AnimalPredators.cpp`, `src/ECS/Life.cpp`) |
| A hunter that can't reach its prey (a villager indoors) gives it up | done | a target no longer available is dropped at the turn's start, and a villager at home is out of the map cells the search walks (`src/ECS/AnimalPredators.cpp`) |
| A hunter stalks and hides before it pounces | done | the stalk from 100 to 50 m and the wolves' hiding in their lair (`src/ECS/AnimalPredators.cpp`) |
| Hunters have a lair near the forests, where they hide and sleep | done | the tiger's and wolf's lairs from the forests and big forests, the lion and leopard where they are (`src/ECS/AnimalLairs.cpp`) |
| Lions frighten creatures | todo | no creature fear of lions was found |
| Other animals flee from hunters | done | the flee-from-predator reaction, made when a predator is created (`src/ECS/AnimalFlee.cpp`) |
| Hunters attack villagers and livestock | done | villagers and other animals are prey (`src/ECS/AnimalPredators.cpp`); see ../villager/death.md |

## Grazers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grazers look for good grass, wander there and graze | done | `LookForFoodPos`, the wander and the eating states (`src/ECS/AnimalAI.cpp`) |
| Grazers drink at the land's drinking places | n/a | Our wiki differs: animals have no thirst and never drink ([animals](../../bw1-notes/animals.md#herbivores-sheep-tortoise-cow-horse-pig)) |
| Grazers flock together and follow their leader | done | the flock steering and the leader (`src/ECS/AnimalAI.cpp`, `components::Flock`) |
| Tortoises (unconfirmed what sets them apart) | done | a grazer of the shared class with its own table values (0.25 m/s); no meat, so it is no prey, and it takes no reactions (`src/ECS/AnimalAI.cpp`) |
