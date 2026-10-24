# Livestock

The animals that belong to towns: sheep, cows, horses, pigs and goats. They graze near their town, breed, are tended by
shepherds and breeders and are slaughtered for food.

**Progress: 6/16 done, 0 chosen, 6 partial, 1 inferred — 65%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../../beings/animal-life.md), [Skeletal animation (villagers and animals)](../../../beings/animation.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place livestock in flocks near towns | done | CREATE_FLOCK and CREATE_ANIMAL (`src/LHScriptX/FeatureScriptCommands.cpp`), the flock on its town's list; the same code as the birds' (testbed `birds.every_kind`) |
| An animal belongs to the town its land script names (a script's CREATE: the nearest town within 500 m); only grazers keep a town, and the flock of any other kind leaves its town | done | `AnimalArchetype::Create`, `CreateAlone` (`src/ECS/Archetypes/AnimalArchetype.cpp`), `CREATE_ANIMAL`; `AnimalScriptRules.AScriptsAnimalKeepsToASmallDomainTakesTheNearestTownThenIsHeld`, `FlocksTest.AnimalSetTownListsAtTheHeadAndForgetUnlinks`; [map script reader](../../../scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal) |
| Livestock graze the land around their town, moving between grazing places | partial 75% | the herd's wander, grazing and the leader's moves within its domain (`src/ECS/AnimalAI.cpp`); `AnimalRules.AGrazersFoodSearchGoesRoundATenthOfItsDomainSquaredInFloats`; the grazing spot refuses a cell under a building, rock or feature (`AnimalBehaviourTest.AGrazerPassesOverACellUnderAManyCelledFixedThing`). |
| Livestock sleep when tired, standing at their flock's sleep cell, with no day or night | done | the sleep need and the sleep at the flock's sleep cell (`src/ECS/AnimalAI.cpp`), as the exe's grazers' needs (none without a sleep cell) ([animals](../../../beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig)); test `AnimalAgeTest.ATiredGrazerGoesToSleepAtItsSleepCellByDayOrNight` (`test/test_animal_age.cpp`) |
| A flock keeps together round its leader | partial 90% | a member far from the leader goes back to its side; the flock steering (`src/ECS/AnimalAI.cpp`, `components::Flock`); the leader is pinned (`AnimalSystemTest.TheLeaderIsTheFirstMemberStillThere`), the steering is not |
| Animals grow up from young, growing by age | done | the young grow four times a year until adult size (`src/ECS/AnimalAI.cpp`); `AnimalAgeTest.AYoungAnimalGrowsUp` |
| Animals breed when their need to breed is met, giving birth to young | done | the breeding need and `GIVES_BIRTH` when the flock is short (`src/ECS/AnimalAI.cpp`); as it reacts to its needs a grazer counts towards breeding for any adult, so a lone survivor refills its herd, as the original; test `AnimalAgeTest.AnAdultGrazerCountsTowardsBreedingAsItReactsToItsNeedsAloneOrInAFullHerd` (`test/test_animal_age.cpp`) |
| A breeder disciple breeds a town's livestock | pending | no breeder disciple; see `../villager/` |
| Shepherds tend a town's flocks, lead them to water and take animals to be slaughtered for food | pending | no shepherds (the original moves the flock's home to its abode's nearest water, `ShepherdMoveFlockToWater`); issue #77; see `../villager/` |
| A slaughtered animal becomes food for its town | pending | see `../resources/food.md` |
| Animals of another player's town count as theirs | inferred | unverified what an owner changes: two flocks of different players never merge (not checked by ours, see [animal behaviour](animal_behaviour.md)); no other reader was looked for |
| The hand picks up livestock, gives them to a town or throws them | done | picked up by the species' rule, thrown, and given to a store as food (`src/ECS/HeldApply.cpp`, `src/ECS/ResourceStores.cpp`); `HeldClassApply.AnAnimalIsGivenOnlyToAStoragePit`, `HeldApplyTest.AnAnimalThrownIntoAPitIsItsFoodWithTheThrowersPlayer`; see ../hand/ |
| The creature eats livestock and plays with them | partial 40% | the creature eats a live animal by its food value (`EatAlive`, `CreatureObjectActionSystem::FoodValueOf`), without the food kinds; playing with them is not checked; see `../creature/` |
| Each kind plays its own clips: stand, move, eat, sleep, in hand, thrown, landed, dying, dead | partial 80% | the clips per state and species (`src/ECS/AnimalAnimations.cpp`); `test/test_living_first_clip.cpp`, `LivingLanding.LandedClips`. Missing: the moving clip plays by the clock, not by the ground; an animal in a dance plays its move clip, not the dance clip; a held one-shot clip's sound events are not replayed; the first clip is chosen in the frame after the animal is made (issue #12) ([animal life, Pending](../../../beings/animal-life.md#pending)) |
| Livestock are prey for wild hunters | done | the predators stalk, chase, pounce on and eat other animals (`src/ECS/AnimalPredators.cpp`); testbed `animals.predators_village` |
| Scripts make, move and read flocks | done | FLOCK_CREATE, FLOCK_ATTACH, FLOCK_DETACH, FLOCK_DISBAND, FLOCK_MEMBER (`src/CHLApi.cpp`, `src/ECS/ScriptContainers.cpp`); `test/test_script_flocks.cpp` |
