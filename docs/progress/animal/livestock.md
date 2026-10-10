# Livestock

The animals that belong to towns: sheep, cows, horses, pigs and goats. They graze near their town, breed, are tended by
shepherds and breeders and are slaughtered for food.

**Progress: 10/16 done, 2 partial — 69%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../bw1-notes/animals.md), [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place livestock in flocks near towns | done | CREATE_FLOCK and CREATE_ANIMAL (`src/LHScriptX/FeatureScriptCommands.cpp`), the flock on its town's list |
| An animal belongs to the town it is near when made, and to that town's player | partial | the animal's town comes from the script's town id (`CREATE_ANIMAL`, `src/ECS/Archetypes/AnimalArchetype.cpp`), not from the town it is near |
| Livestock graze the land around their town, moving between grazing places | done | the herd's wander, grazing and the leader's moves within its domain (`src/ECS/AnimalAI.cpp`) |
| Livestock drink and sleep | done | the sleep need and the sleep at the flock's sleep cell (`src/ECS/AnimalAI.cpp`). Our wiki differs: animals have no thirst and never drink ([animals](../../bw1-notes/animals.md#herbivores-sheep-tortoise-cow-horse-pig)) |
| A flock keeps together round its leader | done | a member far from the leader goes back to its side; the flock steering (`src/ECS/AnimalAI.cpp`, `components::Flock`); `test/test_flock.cpp` |
| Animals grow up from young, growing by age | done | the young grow four times a year until adult size (`src/ECS/AnimalAI.cpp`); `test/test_animal_age.cpp` |
| Animals breed when their need to breed is met, giving birth to young | done | the breeding need and `GIVES_BIRTH` when the flock is short (`src/ECS/AnimalAI.cpp`) |
| A breeder disciple breeds a town's livestock | todo | no breeder disciple; see `../villager/` |
| Shepherds tend a town's flocks and take animals to be slaughtered for food | todo | the shepherd's states are TODO rows; see `../villager/` |
| A slaughtered animal becomes food for its town | todo | see `../resources/food.md` |
| Animals of another player's town count as theirs (unconfirmed what that changes) | todo |  |
| The hand picks up livestock, gives them to a town or throws them | done | picked up by the species' rule, thrown, and given to a store as food (`src/ECS/HeldApply.cpp`, `src/ECS/ResourceStores.cpp`); see ../hand/ |
| The creature eats livestock, plays with them, or herds them (unconfirmed herding) | partial | the creature eats animals by their food value (`CreatureObjectActionSystem::FoodValueOf`); playing and herding: see `../creature/` |
| Each kind plays its own clips: stand, move, eat, sleep, in hand, thrown, landed, dying, dead | done | the clips per state and species (`src/ECS/AnimalAnimations.cpp`) |
| Livestock are prey for wild hunters | done | the predators stalk, chase, pounce on and eat other animals (`src/ECS/AnimalPredators.cpp`) |
| Scripts make, move and read flocks | done | FLOCK_CREATE, FLOCK_ATTACH, FLOCK_DETACH, FLOCK_DISBAND, FLOCK_MEMBER (`src/CHLApi.cpp`, `src/ECS/ScriptContainers.cpp`); `test/test_script_flocks.cpp` |
