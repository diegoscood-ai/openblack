# Birds

The flying animals: crows, doves, swallows, pigeons, seagulls and bats (the vulture's kind makes nothing: [not in the
original](../../not-in-the-original.md#animalbirds)), flying in flocks over the land, and
the doves or bats that circle each temple. The flock miracle's doves and bats are in `../miracles/`.

**Progress: 11/13 done, 0 chosen, 2 partial, 0 inferred — 98%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../../beings/animals.md), [Skeletal animation (villagers and animals)](../../../beings/animation.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make flocks of birds and the birds in them | done | CREATE_FLOCK, CREATE_ANIMAL and CREATE_NEW_ANIMAL (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AnimalArchetype.cpp`); testbed `birds.every_kind`, `birds.many_flocks`; test `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` (`test/test_animal_behaviour.cpp`) |
| Each kind has its own models, clips (fly, stand, eat, sleep, thrown, dead) and table values | done | the table values and each kind's clips, flapping or gliding at random (`BirdClip`, `src/ECS/AnimalAnimations.cpp`); `LivingFirstClip.ABirdsFirstClipIsItsMoveClipDrawnAsTheOriginalDraws`, `LivingFirstClip.TheBirdsAreTheNineWithTheirOwnClipRules` |
| A bird is born at a random age and sized for it | done | the random age and the scale for it (`src/ECS/AnimalAI.cpp`); `test/test_animal_age.cpp`, `AnimalSystemTest.ScaleAndRadius` |
| Doves circle a good player's temple, bats an evil one's, changing as the alignment changes | done | every 100 turns a bird more or fewer, up to the heart's most by the alignment, the other kind gone at once (`src/Worship/Citadel.cpp`, `src/Animals/BirdRules.cpp`); `test/test_bird_rules.cpp` (`BirdRules.ATemplesBirdsGrowWithHowFarItsPlayerIsFromNeutral` and the rest); testbed `birds.temple_doves`, `birds.temple_bats`; [magic](../../../magic/temple-and-citadel.md#the-temples-doves-or-bats) |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flock's leader flies legs about where the flock lives, between its kind's inner radius and the flock's reach | done | the leader's legs of up to the domain radius from the flock's home (`src/ECS/AnimalBirds.cpp`); `BirdRules.ABirdGoesOnlyWhereLandBlocksLieAllTheWayBack`; testbed `birds.every_kind`, `birds.close_up` |
| Followers fly to points near the leader, then keep a formation behind it by their place in the flock | partial 90% | the followers' point near the leader and their formation slot (`src/ECS/AnimalBirds.cpp`); the slot and its point are `animals::birds::FormationSlotOf` / `FormationGoal`, pinned to the exe's dove on the oracle (`BirdRules.AFollowersSlotGrowsOutwardsSquareBySquareFromItsPlaceInTheList`, `BirdRules.AFollowerFliesToItsSlotTurnedByTheWayToItsLeader`, `test/test_bird_rules.cpp`); not pinned: the first point near the leader; a follower whose top state is not the flock's returns, where the original goes on to its decision ([animals, Pending](../../../beings/animals.md#pending)) |
| Birds keep a height above the land within their kind's band, climbing slowly | done | the height band, the slow climb and never under 2 m (`src/ECS/AnimalBirds.cpp`); a new leg's height starts from the last leg's goal height, as the original's (`animals::birds::NextLegAltitude`, test `test/test_bird_rules.cpp` `BirdRules.ABirdsNextLegVariesAboutItsLastGoalsHeightWithinItsKindsLimits`); the climb and the 2 m floor are `animals::birds::LowGoalStep` / `HighGoalStep` (test `BirdRules.ABirdClimbsOrSinksByItsKindsChangeAndNeverFliesUnderTwoMetres`) |
| Birds turn by at most their kind's turn angle and bank into turns | done | the turn angle and the banking through the drawn matrix (`src/ECS/AnimalBirds.cpp`, `src/ECS/MobileDrawing.cpp`); testbed `birds.close_up`; test `AnimalBirdBanking.aBirdLeansHalfARadianOverTwoSecondsAndTheSpellsBirdsOverHalfASecond` (`test/test_animal_behaviour.cpp`) |

## Birds and the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bats and vultures frighten creatures | done | the animals' part of what frightens a creature (`AnimalSystem::IsFrighteningToCreature`), read by the creature's fear (`CreatureMindLearning.cpp`); `AnimalSystemTest.BatsVulturesBigCatsAndWolvesFrightenCreatures`; [mind](../../../creature/mind.md#what-frightens-a-creature-and-the-fire-it-believes) |
| A bird killed falls out of the sky | done | a dying bird goes into the physics with its flight speed along its heading (`BirdDying`, `src/ECS/AnimalBirds.cpp`) ([animals](../../../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)); test `AnimalBehaviourTest.ABirdKilledFallsWithItsFlightSpeedAlongItsHeading` (`test/test_animal_behaviour.cpp`: the velocity's bits for six speeds and headings, as the exe's dove gives them on the oracle) |
| A dead bird lies its time, then goes in a puff of grey smoke | done | the corpse's time and the carcass's smoke, as every animal (`src/ECS/AnimalAI.cpp`, `src/ECS/DisappearSmoke.cpp`); testbed `birds.dead_doves`; tests `AnimalBehaviourTest.ABodyKilledAgainLiesItsFullTimeAfresh`, `VillagerDeathPure.DeadTick`, `BirdRules.ADeadBodysSmokeRisesHalfItsHeightOutTheWayItFaces` |
| Tornadoes carry birds off | partial 90% | the tornado carries any animal it can take up (`src/Particles/Rules/Storm.cpp`); the birds' class answers the take-up tests as every animal, as the exe's: it can become a physics object (`ParticleCarriedObjectsTest.EveryBirdCanBecomeAPhysicsObjectLikeAnyOtherAnimal`) and a miracle may destroy it (`Explosion.aMiracleMayDestroyEveryBirdAsAnyOtherAnimal`); the take-up order and the funnel fit are `Storm.theTornadosPickUpOrderAndReach` / `Storm.onlyAThingSmallerThanTwiceTheBottomAndThanTheTopFitsTheFunnel`; no test or scenario carries a flying bird through the tornado |
| A creature picks up a bird only when it is under a metre above the land, one that has fallen | done | `animals::birds::CreatureCanPickUp`, asked by the creature's pick-up (`AnimalSystem::CanBePickedUpByCreature`, `CreatureObjectActionSystem.cpp`); `BirdRules.ACreaturePicksUpOnlyABirdUnderAMetreAboveTheLand`; testbed `birds.dead_doves`. What the creature then does with it: see `../creature/` |
