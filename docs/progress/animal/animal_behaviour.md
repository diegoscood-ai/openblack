# Animal behaviour

What all animals share: their needs, how they move and react, how they are drawn, and how they die.

**Progress: 12/31 done, 0 chosen, 15 partial, 2 inferred — 78%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../../beings/animal-life.md), [Skeletal animation (villagers and animals)](../../../beings/animal-life.md).

## Needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals get hungry and tired, and need to breed, each at its kind's rate | partial 90% | the needs counters (hunger, sleep, breeding) up to their info.dat maximum, in the states that have the needs flag (`src/ECS/AnimalAI.cpp`), as the wiki reads the original ([animals](../../../beings/animals.md#turn)); no test pins the counters. There is no thirst in the original |
| A hungry grazer looks for a grazing spot in a spiral in front of it, a tired one sleeps at its flock's sleep cell, and a flock short of members breeds | partial 70% | the spiral, the sleep cell and the birth (`src/ECS/AnimalAI.cpp`); the spiral's length is pinned (`AnimalRules.AGrazersFoodSearchGoesRoundATenthOfItsDomainSquaredInFloats`). Missing: the spot does not refuse a cell with a fixed object (audit D6), and the breed count repeats the needs' conditions, so a lone survivor never breeds (D7) ([animals, Pending](../../../beings/animals.md#pending)) |
| Animals eat what their kind eats: grazers graze, hunters eat the animals and villagers they bring down, and a food pile draws only the hungry ones whose food kinds take it | partial 90% | `src/ECS/AnimalAI.cpp`, `src/ECS/AnimalPredators.cpp` (prey only villagers and animals), `src/ECS/AnimalFlee.cpp` (the pile's reaction); no test pins the food kinds |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal turns by at most its kind's turn angle, harder close to its goal, and steps its speed | partial 90% | the turn by at most the kind's turn angle, harder inside its turning circle (`src/ECS/AnimalAI.cpp`); the step is pinned (`LivingTurnTest.InitStepsXZIsTheExesIntegerStep`), the turn is not |
| It goes only where it can reach without circling | partial 90% | the turning circle rule (`IsPosValidForTurnAngle`, `src/ECS/AnimalAI.cpp`); no test pins it |
| Its clip plays by the ground it covers while moving, else by the clock | partial 50% | the clip per state and species (`src/ECS/AnimalAnimations.cpp`), by the clock; the moving clip plays by the clock too, since only villagers carry a ground speed (`src/ECS/Animations.cpp`; [animal life, Pending](../../../beings/animal-life.md#pending)) |
| It is drawn between its last two turns | done | the smooth drawing between turns (`src/ECS/MobileDrawing.cpp`); `test/test_drawn_position.cpp` |
| It looks round as it goes, within its view angle | partial 90% | the grazing search in front, within half its view angle each side (`src/ECS/AnimalAI.cpp`); no test pins the angle |
| It walks round obstacles and buildings | partial 70% | the detour round objects (`src/ECS/AnimalWallHug.cpp`); `test/mobile_wall_hug/test_mobile_wall_hug.cpp`, `test/test_wall_hug_rules.cpp`; testbed `animals.orbit_goes_round`, `animals.orbit_leaves_at_once`. Missing: the walk round a circle is a float orbit, not the original's whole-unit steps and sweep along the circle; trying both ways round and the look-ahead are not ported; positions and goals are not kept in whole map units (issue #82; [wall hugging, Pending](../../../beings/wall-hugging.md#pending)); see `../physics/` |
| Ground animals keep their feet on the land and cast a ground blob shadow | done | on the land, with their blob shadows (`src/Graphics/GroundBlobs.cpp`, `Renderer::DrawGroundBlobs`); `test/test_ground_blobs.cpp` |
| A wandering animal goes straight and only turns on entering another cell | partial 70% | `src/ECS/AnimalAI.cpp`; the move only refuses at the land's edge, where the original makes up to three tries along the axes against the cells' bounds and collisions (audit D10); and an animal counts its turn in a state before its reaction is processed, the other way round from the original (D11) ([animals, Pending](../../../beings/animals.md#pending)) |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals run from a frightening thing, and hunters run from what the hand throws close by | partial 70% | reactions 0 and 9 (`src/ECS/AnimalFlee.cpp`, `animal_ai::SpreadFlyingObjectReaction`). Missing: a second reaction clears the current one before it starts, so the saved state is lost, and the same-type gate is missing (audit D1); a thing that did not move gives no direction where the original gives its step (D4); looking at the thing turns at once where the original turns a step a turn (D9); no test pins reaction 9 |
| Animals run from fire | pending | reaction 10 is not ported for animals (`src/ECS/AnimalFlee.cpp`) |
| Animals look at miracles and flee the frightening ones | pending | the look and spell reactions (1, 3) are not ported for animals (`src/ECS/AnimalFlee.cpp`) |
| Hungry grazers go to a food pile the hand or a store puts down, eat 50 of it and are no longer hungry | partial 90% | `animal_ai::SetupPotReaction`, `GotoFoodReaction`, `ArrivesAtFoodReaction` (`src/ECS/AnimalFlee.cpp`, `src/ECS/AnimalAI.cpp`); testbed `animals.orbit_goes_round`; no test pins the 50 and the hunger. The magic food piles draw no animal, in the original too ([animal life](../../../beings/animal-life.md#reactions)) |
| Animals react to being picked up and dropped by the hand | done | `IN_HAND` with its clip, then the flight and the landing (`src/ECS/AnimalAI.cpp`, `src/ECS/Systems/Implementations/VillagerPhysics.cpp`); `LivingTurnTest.TheHandLetsGoOnlyIntoAFlightALandingOrADeath`, `test/test_living_landing.cpp` |
| Animals run from a creature near them, longer for a near creature and an impressive last action | done | `src/ECS/AnimalFlee.cpp`, made by every creature as it is made (`src/ECS/Archetypes/CreatureArchetype.cpp`); priorities, ways and turn counts pinned to the original's numbers in `test/test_animal_creature_reactions.cpp` |

## Hand, physics and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks up the animals the table allows; held, they play their in-hand clip | done | by the species' `playerCanPickUp`, leaving its flock, with the in-hand clip (`src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp`); `AnimalSystemTest.TheHandPicksUpWhatTheSpeciesAllows`; see ../hand/picking_up.md |
| Thrown animals fly, land and play their landed clip, hurt by a hard landing | done | `FLYING`, the landing pose, heading and clip, hurt by a hard landing (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`, `src/ECS/AnimalAI.cpp`); `LivingLanding.AnimalLandTypeIsReversed`, `LivingLanding.LandedClips`, `LivingCrush.OnlyAKnockStrictlyAboveTwiceItsWeightHurts`; see ../physics/thrown_living.md |
| An animal picked up leaves its flock for one of its own; after landing, its flock joins a nearby one of its kind when the two fit | partial 80% | `flocks::SeparateIntoNewFlock`, `LookForFlocksInSpiral` (`src/ECS/Flocks.cpp`, `src/ECS/AnimalAI.cpp`); `FlocksTest.SeparateKeepsTheDomain`, `FlocksTest.MergeTakesTheOthersFromItsHead`. Missing: the merge looks only at its own flock's village, with no shepherd nor flock player test ([animal life, Pending](../../../beings/animal-life.md#pending)) |
| Animals drown in deep water | partial 80% | an animal in the sea sinks and is deleted (`src/ECS/VillagerDrowning.cpp`); missing: the test that it is available, and the report to the creature of the hand that dropped it (throwing into the sea) (issue #84; [animal life](../../../beings/animal-life.md#hand-flight-and-death)) |
| Hurting or helping an animal moves the player's alignment (kind or nasty) | partial 90% | a miracle's effect on an animal moves its caster's alignment by its kind's row (`alignment::Update`, `src/ECS/Effects/Alignment.cpp`); no test pins it for an animal |
| Scripts take control of an animal, play its clips and let it go | done | the script states, MOVE_GAME_THING and the release (`src/ECS/AnimalScript.cpp`, `src/ECS/ScriptHeld.cpp`); leaving IN_SCRIPT it lets go of the circle it went round and goes only into a script state, one a script may interrupt or the hand (`CallExitStateFunction`, `src/ECS/AnimalAI.cpp`; [animals](../../../beings/animals.md#the-exit-functions)); `AnimalSystemTest.LeavingAScriptLetsGoOfTheCircle`, `test/test_release_from_script.cpp`, `AnimalScriptRules.*` |

## Death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal with no life left falls dying, playing its kind's dying clip once, then lies in its dead clip | done | `animal_ai::DestroyedByEffect` and the dying and dead clips (`src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp`); testbed `animals.dead_body_smoke` |
| A dead animal lies 600 turns and one more, then goes | done | the shared corpse counter (`living::DeadTick`), 600 turns; `VillagerDeathPure.DeadTick`; testbed `animals.dead_body_smoke` |
| Killed again, its body only lies its full time afresh | partial 90% | `animal_ai::DestroyedByEffect` restarts the corpse's time; no test pins it |
| It goes in a puff of grey smoke | done | the carcass's smoke (`src/ECS/DisappearSmoke.cpp`, from `src/ECS/AnimalAI.cpp`); `BirdRules.ADeadBodysSmokeRisesHalfItsHeightOutTheWayItFaces`; testbed `animals.dead_body_smoke` |
| Miracles hurt and heal animals as other living things | done | `effects::ApplyEffect` hurts and heals them through `ecs::life` (`src/ECS/Effects/EffectValues.cpp`); `test/test_living_impact.cpp` |
| Fire burns animals to death | done | the fire's damage and `fire::traits::DestroyedByEffect` to dying (`src/ECS/Fire/FireObjectTraits.cpp`); testbed `animals.dead_body_smoke`; see ../physics/fire.md |
| An animal sacrificed at a worship totem goes at once | inferred | `altar_offering::Offer` deletes it (`src/ECS/AltarOffering.cpp`; `AltarSacrifice.AnAnimalIsWeightedByItsLifeAndTakenWhateverTheSite`); unverified: the original keeps the death reason Sacrifice and deletes the body in its dead state with its puff of smoke, which ours never keeps (`Dead` in `src/ECS/AnimalAI.cpp` passes no reason), and whether that shows a dying turn |
| A dead animal is food for hunters and the creature | inferred | predators eat what they bring down (`src/ECS/AnimalPredators.cpp`); unverified: whether a hunter takes a body it finds dead (the pounce takes any living thing, but a body's dead state refuses to leave), and the creature's eating of bodies (see ../creature/) |
