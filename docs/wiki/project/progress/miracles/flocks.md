# Flocks

Two flock miracles are swept out of the hand. The flying flock makes doves for a good god or bats for an evil one; they
fly off together, leave a trail and frighten creatures (bats). The ground flock makes a pack of wolves that run down a
corridor, hunting and eating villagers in their way. Wild animals and livestock are in [../animal/](../animal/).

**Progress: 20/50 done, 0 chosen, 27 partial, 0 inferred — 83%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/flocks.md).

## Flying flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Doves for a caster whose alignment is good enough, bats otherwise | done | `src/Magic/Spells/SpellFlock.cpp` (the caster's alignment under the switch, -0.2: bats); test `SpellFlock.goodOrEvil` |
| Twelve birds times tribal power, rounded half to even | done | `src/Magic/Spells/SpellFlock.cpp` (numberToCreate x tribal power, rounded to nearest even); test `SpellFlock.numberToCreate` |
| Birds are made twelve a second along the hand's sweep, their height eased along it, each nudged a little at random | done | `src/Magic/Spells/SpellFlock.cpp` (the exit loop: twelve a second along the last point to the hand, the running total summed in floats as the original's FPU sums it, height interpolated, a 0.1 m jitter, both in floats as the original's FPU rounds them); test `SpellFlock.spawnPoint`, test `test/test_flock_miracle.cpp` `FlockEmit.TheTotalStepsInFloatsAsTheGamesFpuSumsIt`, `FlockCasting.AllTwelveBirdsAreMadeInTheTenthTurnAndNoMore` (six by the fifth turn, eight by the sixth), `FlockBirth.ABirthPointIsNudgedInFloatsAsTheGamesFpuRoundsIt`, `FlockBirth.ABirthHeightIsEasedInFloatsAsTheGamesFpuRoundsIt` |
| Birds that would start off the map, or outside the human caster's influence, are skipped but still counted | done | `src/Magic/Spells/SpellFlock.cpp` (off the map, or a human caster with no influence there: skipped, still counted); test `test/test_flock_miracle.cpp` `FlockCasting.BirdsThatWouldStartOffTheMapAreSkippedButCounted`, `FlockCasting.BirdsOutsideAHumanCastersInfluenceAreSkippedButCounted`, `FlockCasting.OnlyAHumanCasterNeedsInfluenceForItsBirds` |
| Birds keep being made until all are, even after the miracle is over | done | `src/Magic/Spells/SpellFlock.cpp` (the exit loop runs while fewer are made than wanted, closed or not, as the original's turn, which tests only the count); test `test/test_flock_miracle.cpp` `FlockCasting.BirdsKeepBeingMadeAfterTheMiracleIsOver`, `FlockCasting.AllTwelveBirdsAreMadeInTheTenthTurnAndNoMore` |
| They fan out across the camera's heading (or the creature's throw), side chosen by which way the hand swept | done | `src/Magic/Spells/SpellFlock.cpp` (the destination: the camera's heading for a human, side from the sweep, angle 2 x made x side / N); test `SpellFlock.fan` |
| Each flies for a point up to 800 m away in whole map cells, halving the distance down to 12.5 m until it is on the map | done | `src/Magic/Spells/SpellFlock.cpp` (whole cells, halved down to 12.5 m until on the map); test `SpellFlock.destination` |
| Birds fly at their kind's normal height; followers take the leader's goal | done | `src/Magic/Spells/SpellFlock.cpp` (the leader flies at its kind's normal height; the others take the leader's goal) Test `SpellFlock.realInfoDat` (altitude, with the game's data); scenario `miracles.flock_doves` |
| Every bird is the same size, 2.8 to 3.0 times the model, whatever its birth size | done | `src/Magic/Spells/SpellFlock.cpp` (2.8 plus up to 0.2 times the model's scale, the exe's constants); test `test/test_flock_miracle.cpp` `FlockCasting.EveryBirdIsTwoPointEightToThreeTimesItsModelWhateverItsBirthSize` |
| A bird faces its goal from where it actually appeared | partial 80% | `src/Magic/Spells/SpellFlock.cpp` (the angle from the jittered birth point to the goal) No test covers it |
| The local caster's hand leaves a trail of sparkles (good) or smoke (evil) until all birds are made | done | `src/Magic/Spells/SpellFlock.cpp` (the cast effect follows the local hand and is stepped every frame, closed when all are made) Scenarios `miracles.flock_doves`, `miracles.flock_bats`; the trail is stepped every frame, as the original; test `test/test_flock_miracle.cpp` `FlockCasting.TheLocalCastersHandLeavesSparklesUntilEveryBirdIsMade`, `FlockCasting.AnEvilCastersHandLeavesSmokeAndMakesBats`, `FlockCasting.OnlyTheLocalCastersHandLeavesATrail` |

## Flying flock: the birds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader wanders from wherever it is now, so the flock roams away from the cast point | done | `src/Magic/Spells/SpellFlock.cpp` (the flock is placed where its leader is each turn), `src/ECS/AnimalBirds.cpp`. Scenario `miracles.flock_doves` |
| Each leg's height varies within the kind's limits | done | `src/ECS/AnimalBirds.cpp` (about the last leg's goal height, as the original, `animals::birds::NextLegAltitude` in `src/Animals/BirdRules.cpp`), `src/ECS/AnimalAI.cpp`; test `test/test_bird_rules.cpp` `BirdRules.ABirdsNextLegVariesAboutItsLastGoalsHeightWithinItsKindsLimits` |
| Random wander points avoid places the map's walls block | partial 80% | `src/ECS/AnimalAI.cpp` (the random point is refused where it collides, `living::Collides` in `src/ECS/LivingPos.cpp`) No test covers it |
| Birds bank into their turns and flap | done | `src/ECS/AnimalBirds.cpp`, `src/ECS/AnimalAnimations.cpp`. Test `UrFlocking.banking` |
| Birds are lit by the brightest land light | partial 80% | `src/ECS/Systems/Implementations/RenderingSystem.cpp` (the land light on objects) No test covers it |
| A bird crossing a shield into it strikes the shield, sparks, and fades if the shield holds | partial 80% | `src/Magic/Spells/SpellFlock.cpp` (`particles::shields::FindShieldCrossedInto` with the bird's 2D radius, a shield-collide event, then the fade if the shield holds) The previous position used for the shield test is the spell's previous turn, approximate ([wiki](../../../magic/flocks.md#openblack); issue #59); no test |
| When the miracle ends every bird fades over 20 turns, but stays opaque until it vanishes | done | `src/Magic/Spells/SpellFlock.cpp` (the fade zoomer over 20 turns, the alpha only changes in the turn); test `SpellFlock.fade`. Scenario `miracles.flock_doves_vanish` |
| The flying flock costs 40 a turn while its birds or effect last; from a hand seed its first 5000 prayer runs out after 12.5 s, before its 25 s timer | done | The tables' timer and cost per turn (`src/Magic/Spells/SpellFlock.cpp`), as the original ([page](../../../magic/flocks.md#pending)); test `SpellFlock.realInfoDat` |
| Bats frighten creatures | pending | Nothing in our tree asks whether an animal frightens a creature |

## Flying flock: effects and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each bird trails pale sparkles (doves) or black smoke (bats), sized to the bird | done | `src/Particles/Rules/Flock.cpp` (follow targets: a trail on each bird, scaled by it) Scenarios `miracles.flock_doves`, `miracles.flock_bats` |
| A trail stays where its bird was when the bird goes, and fades with the effect | partial 80% | `src/Particles/Rules/Flock.cpp` (a target that goes is forgotten, its atom stays) No test covers it |
| One coo (doves) or screech (bats) per cast, not one per bird | done | `src/Particles/Rules/Flock.cpp` (`flocking::PlaysCreateSound`: with SoundOneOnly, which the files leave at its default 1, only the collection's first atom, [page](../../../magic/flocks.md)); tests `test/test_flock.cpp` `SpellFlock.aFlockSoundsOncePerCast`, `SpellFlock.realDataEachFlockSoundsOncePerCast` |

## Ground flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Always wolves, whatever the caster's alignment | done | `src/Magic/Spells/SpellFlock.cpp`. Scenario `miracles.flock_wolves` |
| Fourteen wolves times tribal power, rounded half to even | done | `src/Magic/Spells/SpellFlock.cpp`; test `SpellFlock.numberToCreate` |
| Made twelve a second along the sweep on the ground, skipped off the map or outside influence | done | `src/Magic/Spells/SpellFlock.cpp` (the ground exit loop, its running total summed in floats as the original's FPU sums it); test `test/test_flock_miracle.cpp` `FlockCasting.AllFourteenWolvesAreMadeInTheEleventhTurnOnTheGround`, `FlockCasting.WolvesOffTheMapOrOutsideAHumanCastersInfluenceAreSkippedButCounted`, `FlockEmit.TheTotalStepsInFloatsAsTheGamesFpuSumsIt` |
| A puff of magic appears where each wolf appears | done | `src/Magic/Spells/SpellFlock.cpp` (a magic-object-created spot visual at each wolf) Scenario `miracles.flock_wolves` |
| Every wolf is 1.5 to 2.0 times the model, whatever its birth size | done | `src/Magic/Spells/SpellFlock.cpp` (1.5 plus up to 0.5 times the model's scale, the exe's constants); test `test/test_flock_miracle.cpp` `FlockCasting.EveryWolfIsOnePointFiveToTwoTimesItsModelWhateverItsBirthSize` |
| Wolves are born grown up and hungry, owned by the caster | done | `src/Magic/Spells/SpellFlock.cpp` (made an adult, grown-up age plus one, with the info's hunger, owned by the caster); test `test/test_flock_miracle.cpp` `FlockCasting.WolvesAreBornGrownUpHungryAndTheCastersAndCannotBePickedUp`, `FlockAnimalRows.realInfoDat` (age 13 + 1, hunger 150) |

## Ground flock: running and hunting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack runs down a 45 m-wide corridor to its goal at the wolves' run speed | done | `src/Magic/Spells/SpellFlock.cpp` (the corridor's normal and 45 m half width, the run speed); test `SpellFlock.wolfCorridor` |
| Wolves reaching their goal fade away | done | `src/ECS/AnimalPredators.cpp` (within 30 m of the final goal, the fade) Scenario `miracles.flock_wolves_fade` |
| A hungry wolf hunts villagers inside the corridor and not behind it | done | `src/ECS/AnimalPredators.cpp` (IsHuntingTargetValid: inside the corridor, not behind the wolf's cell); test `SpellFlock.wolfCorridor` |
| Once a wolf remembers prey, a new prey takes its place when nearer by the original's mixed units (in practice almost always) | done | `src/ECS/AnimalPredators.cpp` (`animal_ai::detail::PreyNearerThanStored`: 2 x the prey's Chebyshev distance in map units against my cell indices' distance to the stored candidate, [page](../../../beings/animals.md)); test `test/test_animal_behaviour.cpp` `AnimalPredatorRules.ANewPreyReplacesTheStoredOneByTheOriginalsMixedUnits` |
| It chases, pounces when close and facing, and gives up when the prey gets too far | partial 70% | `src/ECS/AnimalPredators.cpp` (chase, stalk, pounce, abandon). Our wiki notes the spell wolves circle 30 to 50 m from the prey and seldom reach the leap; not checked against the game ([page](../../../magic/flocks.md#pending)) Scenarios `miracles.flock_wolves`, `animals.predators_village` |
| Within a metre of the prey the pounce brings it down, again each turn it stays that close | partial 80% | `src/ECS/AnimalPredators.cpp` (TargetPounce: within 1 m the villager is downed at 0.05 life) Not reached in play while the wolves do not get to the leap (row above); no test |
| The downed villager plays its attacked fall | partial 80% | `src/ECS/AnimalPredators.cpp` (ProcessDownedVillagers: the downed clip, then being eaten) Not reached in play while the wolves do not get to the leap; no test |
| The villager is eaten over 300 turns and then dies the normal villager death, killed by an animal | partial 80% | `src/ECS/AnimalPredators.cpp` (300 turns, then the villager's death by an animal, `ecs::villager::VillagerDead`) Not reached in play while the wolves do not get to the leap; no test |
| The wolf takes 15 to 24 mouthfuls, then runs on | partial 80% | `src/ECS/AnimalPredators.cpp`. Not reached in play while the wolves do not get to the leap; no test |
| Things that can't be eaten are never hunted | partial 80% | `src/ECS/AnimalPredators.cpp` (a thing that cannot be eaten, `script_held::CannotBeEaten`, is no prey) No test covers it |
| A remembered prey that can no longer be reached is dropped | partial 80% | `src/ECS/AnimalPredators.cpp` (`ecs::villager::IsReachable` and the other validity checks) No test covers it |

## Ground flock: look, sound and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wolves leave a land-tinted dust trail | done | `src/Particles/Rules/Flock.cpp` (the ground dust trail) Scenario `miracles.flock_wolves` |
| One howl per cast | done | `src/Particles/Rules/Flock.cpp` (`flocking::PlaysCreateSound`, SF_FlockGroundDust's SoundOneOnly left at 1, [page](../../../magic/flocks.md)); tests `test/test_flock.cpp` `SpellFlock.aFlockSoundsOncePerCast`, `SpellFlock.realDataEachFlockSoundsOncePerCast` |
| Wolves bank into their turns and play the right clip in each state | partial 80% | `src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp`. Scenario `miracles.flock_wolves`; the lean is the original's (over 1 s, by its speed times its turn, made positive, x 0.5 / 20000, at most 0.5 rad; `TimeToBank` / `BankAngle` in `src/ECS/AnimalBirds.cpp`), test `test/test_flock_miracle.cpp` `FlockWolves.TheSpellWolvesLeanByTheirSpeedAndTurnOverASecond`; the clips have no test |
| When the miracle ends the wolves turn translucent white and fade over 2 s while running on | done | `src/Magic/Spells/SpellFlock.cpp` (the fade, the animal keeps moving), drawn white with its alpha by `src/ECS/Systems/Implementations/RenderingSystem.cpp`. Test `SpellFlock.fade`; scenario `miracles.flock_wolves_fade` |
| A burning wolf is drawn charred with flames | partial 80% | `src/ECS/Systems/Implementations/RenderingSystem.cpp` (the spell wolf with a fire: the charring grey and the glow); flames from the fire graphics (`src/ECS/Fire/FireGraphic.cpp`) No test or check covers it |
| The player can't pick up the miracle's wolves | done | `ecs::animal_ai::ValidForPlaceInHand` (the species' pick-up rule, the original's for every animal: [picking](../../../hand/picking.md)), used by `src/ECS/Systems/Implementations/HandPlacement.cpp`; test `test/test_flock_miracle.cpp` `FlockCasting.WolvesAreBornGrownUpHungryAndTheCastersAndCannotBePickedUp`, `FlockAnimalRows.realInfoDat` (the wolf's pick-up flag 0) |
| The ground flock costs 35 a turn; from a hand seed its first 5000 prayer runs out after 14.5 s, before its 60 s timer | done | The tables' timer and cost per turn, as the original ([page](../../../magic/flocks.md#pending)); test `SpellFlock.realInfoDat` |

## Both flocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader is a miracle event each turn with no effect on anything | done | `src/Magic/Spells/SpellFlock.cpp` (the leader's point event, reaction to an impressive miracle, [page](../../../magic/flocks.md)); both flocks' effect rows have no burn, crush, hit, heal, throw or belief and the plain alignment modifier 1; integration test `test/test_flock.cpp` `SpellFlock.realInfoDat` |
| A running flock and its animals are kept in a saved game | pending | No game save system |
| A creature can cast either flock, throwing it the way it faces | pending | creatures cast other miracles ([../creature/creature_casting.md](../creature/creature_casting.md)), not the flocks; the creature's direction rule is in `src/Magic/Spells/SpellFlock.cpp` but nothing calls it for a creature |
