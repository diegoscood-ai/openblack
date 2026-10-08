# Creature locomotion

How a creature gets about: it plans a route round what is in its way, walks or runs along it at a speed that suits its
size, the slope and how tired it is, turns and steps off before setting out, and leaves footprints behind. It can wade
through shallows but not deep sea, follow things, run away, travel by teleport, and be carried off by a tornado.

**Progress: 33/50 done, 4 partial — 70%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Speed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walking is 8 and running 20 units a second for each unit of a size-based length; bigger creatures go faster | done | `src/Creature/CreatureLocomotion`, used by `CreatureLocomotionSystem`; test `CreatureLocomotion.SpeedsGrowWithSize` |
| A creature is asked for a fraction of a little over its running speed | done | test `CreatureLocomotion.TargetSpeedIsAFractionOfALittleOverRunning` |
| It speeds up at twelve units a second each second | done | test `CreatureLocomotion.AcceleratesAtTwelveUnitsASecondEachSecond` |
| It brakes so as to stop in the distance left, and slows for corners | done | test `CreatureLocomotion.BrakesToStopInTheDistanceLeft` |
| Uphill is slower and downhill a little faster | done | test `CreatureLocomotion.UphillIsSlowerDownhillALittleFaster` |
| Exhausted, it only goes slowly | done | test `CreatureLocomotion.ExhaustedCreaturesGoSlowly` |
| Pulled on the leash it goes faster, up to twice its running pace | done | `CreatureLocomotionSystem::LeadTo`; test `CreatureLocomotion.PullingTheLeashSpeedsItUp`; see [leash.md](leash.md) |
| Frozen it can't move | partial | the freeze spell (`creature_spells`, `src/Magic/Spells/SpellCreature.cpp`) slows its animation to a stop (`CreatureAnimationSystem`) and the hand can't hold it, but the locomotion does not read it, so a walk goes on (see [../miracles](../miracles/)) |
| Scripts can set a creature's speed (unconfirmed which functions) | todo |  |

## Walking and running on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Below walking speed it blends standing and walking; above, walking and running, keeping in step | done | tests `CreatureLocomotion.SlowerThanWalkingBlendsStandAndWalk`, `FasterThanWalkingBlendsWalkAndRunInStep` |
| The distance covered drives the walk, so the feet keep to the ground | done | `src/Creature/CreatureLocomotion` blend, played by `CreatureLocomotionSystem::Update` |
| Standing still moves no feet | done | test `CreatureLocomotion.StandingStillMovesNoFeet` |
| Before setting off it steps off sideways or back into a walk, walks straight on if nearly facing the way, or turns on the spot | done | test `CreatureLocomotion.StartsByStepWalkOrTurn`; `CreatureLocomotionSystem` steps off along the route |
| Turns and steps blend the 0, 90 and 180 degree animations of a side for the angle | done | test `CreatureLocomotion.AnglesPickTheirPairOfAnimations` |
| At corners too sharp to walk round it stops and starts again | partial | our route follower rounds the corners into arcs, so `CreatureLocomotionSystem` never stops at one; the stop-and-start is only at the start of a walk |
| Between game turns it is drawn moving smoothly | done | `CreatureLocomotionSystem::Update` writes `CreatureDrawPose` (`creature_pose::BetweenTurns`); tests `CreaturePose.BetweenTurns*`, `CreatureLocomotionSystemTest.TheTransformMovesOnlyOnceATurn` |
| It faces down a slope to do some things (unconfirmed which) | todo |  |

## Routes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land is a grid of 512 by 512 cells sorted once loaded: too steep and deep sea are blocked, shallows can be waded | done | `land_avoid::Validate` (`src/3D/LandAvoid`), the creature's walkable mask built when the landscape opens (`Game.cpp`): steep cells and sea whose four corners are at altitude 0 are blocked, shallow water can be waded; tests `LandAvoidNearest.*` |
| Land cut off from the main part of the land is out of reach | done | the `land_avoid` flood fill marks it unreachable |
| A creature stands clear of cells it can't walk on | done | `land_avoid::IsPosValid` at radius 7.1 and `NearestValid`, used by `CreatureLocomotionSystem`; tests `LandAvoidNearest.*`, `CreatureLocomotionSystemTest.NowhereToStandIsAnInvalidDestination` |
| Things in the way are walked round as circles; big creatures walk through small trees and over low things | partial | our route planner (`src/RoutePlanner`, obstacles from `src/ECS/RoutePlanWorld.cpp`) walks round things as circles; trees are always avoided, so no creature walks through small trees; tests `RoutePlanWorldCreatureTest.*` |
| Villagers and animals are never walked round; other creatures only while they stand still; anything burning always | partial | `CreatureMustAvoid` (`src/ECS/RoutePlanWorld.cpp`): villagers, animals and other creatures are never walked round, standing or not; a burning dead tree or forest always is; another player's shield is (test `RoutePlanWorldCreatureTest.AnotherPlayersShieldIsAvoided`) |
| Routes are planned over a finer lattice a budget a turn, straightened where clear, and their corners rounded | done | the original's route planner and follower (`src/RoutePlanner`, `route_planner::RouteFollower`), a few search turns each game turn in `CreatureLocomotionSystem::ProcessTurn`, corners rounded into arcs; tests in `test/test_rplan.cpp`, `test/test_rplan_follow.cpp`, `RouteFollowerCreature.ContextAndRemainingLength`, `CreatureLocomotionSystemTest.AWalkIsPlannedFollowedAndEnded` |
| It goes round cliffs | done | the route planner's square check reads the walkable mask (`CheckSquareFunction` in `src/ECS/RoutePlanWorld.cpp`) |
| With no way there it gives up, puzzled | done | `CreatureLocomotionSystem::ProcessTurn`: a failed plan pulls the puzzled face and plays the confused animation |
| It arrives anywhere on a ring about its destination | done | test `CreatureLocomotion.ArrivalRings` |
| A walk is given a time to arrive before it is given up | todo | no time limit found in `CreatureLocomotionSystem` |
| Stuck or trapped in an enclosed space, it tries to get out (unconfirmed how) | todo | see [decision_making.md](decision_making.md) |
| It uses the land's footpaths (unconfirmed) | todo | the route planner can follow footpaths (`test/test_use_footpath.cpp`), but the creature's walks do not ask for them |
| A villager or animal under its feet can be trodden on | todo | see [object_actions.md](object_actions.md) |

## Going places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walking or running to a point | done | `CreatureLocomotionSystem::MoveTo`; test `CreatureLocomotionSystemTest.AWalkIsPlannedFollowedAndEnded` |
| Walking up to a thing, stopping short by both their sizes | done | `CreatureLocomotionSystem::MoveToObject` |
| Following a thing as it moves, keeping within a distance | done | `CreatureLocomotionSystem::Follow` |
| Running away from a point | done | `CreatureLocomotionSystem::FleeFrom`; tests `CreatureLocomotion.RunsAwayFromTheThreat`, `CreatureLocomotionSystemTest.RunningAwayDrawsOnceFromTheSyncedStream` |
| Turning on the spot to face a point | done | `CreatureLocomotionSystem::TurnToFace`; test `CreatureLocomotionSystemTest.TurnToFaceTurnsOnTheSpot` |
| Led on the leash to the hand | done | `CreatureLocomotionSystem::LeadTo`, from `LeashSystem`; see [leash.md](leash.md) |
| Following the hand, or keeping to the middle of the screen | todo |  |
| Walking to the beach, up a hill, along a ridge | todo | see [idle_behaviour.md](idle_behaviour.md) |
| Travelling by teleport: walking to a stone and jumping to its partner | todo | the teleport miracle moves no creature; see [../miracles](../miracles/) |
| Carried off by a tornado, it faints where it lands | todo | the tornado (`src/Magic/Spells/SpellStormAndTornado.cpp`) takes no creature; see [../miracles](../miracles/) |
| Knocked over by a heavy blow or a falling thing, it falls and gets up again | todo | see [../physics](../physics/) |
| Moved by a script without walking | todo |  |
| Its position is sent to the other players in a network game | todo | see [../multiplayer](../multiplayer/) |

## Footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only creatures leave footprints: a dark paw, hoof or hand on the land under the lower foot at each step | done | `src/Creature/CreatureFootprints`, `FootprintSystem` (from `ECS/CreatureLoop.cpp`); tests `CreatureAudio.FootstepsLeavePrints`, `CreatureFootprints.LowerFootGetsThePrint`, `test/creature/test_footprint_system.cpp` |
| Each species has its own print | done | test `CreatureFootprints.SpeciesCells` |
| Prints are turned with the foot and sized by the creature; the left foot's is flipped | done | tests `CreatureFootprints.TurnedAndSizedPrint`, `LeftFootFlipsThePicture` |
| Prints fade in steps and are gone in about five seconds | done | tests `CreatureFootprints.FadesInStepsOfAtLeast200Ms`, `GoneInAboutFiveSeconds` |
| At most 256 prints at once; new ones are dropped until old ones fade | done | test `CreatureFootprints.FullTrailDropsNewPrints` |
| On the first of April every creature leaves a smiley face | done | test `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize` |
| Footsteps sound by the ground under the foot | done | `CreatureAudioSystem`, the ground from `ecs::sea_cells::GetSurfaceType`; see [animation.md](animation.md) |
