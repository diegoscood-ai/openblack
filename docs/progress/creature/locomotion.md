# Creature locomotion

How a creature gets about: it plans a route round what is in its way, walks or runs along it at a speed that suits its
size, the slope and how tired it is, turns and steps off before setting out, and leaves footprints behind. It can wade
through shallows but not deep sea, follow things, run away, travel by teleport, and be carried off by a tornado.

**Progress: 33/51 done, 0 chosen, 7 partial, 4 inferred — 72%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

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
| Frozen it can't move | partial 30% | the freeze spell (`creature_spells`, `src/Magic/Spells/SpellCreature.cpp`) slows its animation to a stop (`CreatureAnimationSystem`) and the hand can't hold it, but the locomotion does not read it, so a walk goes on (see [../miracles](../miracles/)) |
| Scripts can set a creature's speed | done | the SPEED property: `SET` sets the fraction of its top speed it goes at (`CreatureLocomotion::fraction`, `script::property_rules::CreatureSpeedFraction`, `CHLApi.cpp`), as `Creature::SetSpeedInMetres` does; `GET` reads the speed it is made with ([script commands](../../../scripts/script-commands.md)); tests `ScriptProperties.ACreaturesSpeedIsAFractionOfItsTopSpeedFromNoneToAll`, `ScriptQueryNatives.ACreatureIsAskedForAFractionOfItsTopSpeed` |

## Walking and running on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Below walking speed it blends standing and walking; above, walking and running, keeping in step | done | tests `CreatureLocomotion.SlowerThanWalkingBlendsStandAndWalk`, `FasterThanWalkingBlendsWalkAndRunInStep` |
| The distance covered drives the walk, so the feet keep to the ground | done | `src/Creature/CreatureLocomotion` blend, played by `CreatureLocomotionSystem::Update`; tests `CreatureLocomotion.InItsPenTheWalkStridesAtThePenScale`, `CreatureLocomotionSystemTest.TheLegsAtAShareOfTheTurnAreWhatTheFrameDrawsThen` |
| Standing still moves no feet | done | test `CreatureLocomotion.StandingStillMovesNoFeet` |
| Before setting off it steps off sideways or back into a walk, walks straight on if nearly facing the way, or turns on the spot | done | test `CreatureLocomotion.StartsByStepWalkOrTurn`; `CreatureLocomotionSystem` steps off along the route |
| Turns and steps blend the 0, 90 and 180 degree animations of a side for the angle | done | test `CreatureLocomotion.AnglesPickTheirPairOfSteps` |
| At corners too sharp to walk round it stops and starts again | partial 50% | our route follower rounds the corners into arcs, so `CreatureLocomotionSystem` never stops at one; the stop-and-start is only at the start of a walk |
| Between game turns it is drawn moving smoothly | done | `CreatureLocomotionSystem::Update` writes `CreatureDrawPose` (`creature_pose::BetweenTurns`); tests `CreaturePose.BetweenTurns*`, `CreatureLocomotionSystemTest.TheTransformMovesOnlyOnceATurn` |
| It faces down the slope before it sleeps, rests, sits down or sits on a hill with a friend | partial 50% | the original's agendas for the sleep, the sleep at a place, by an object, the rest to get better, going home to recover, sitting down and sitting on a hill with a friend face down the slope (sub-action 0x44 queued in the v1.42 exe by 0x49B1C0, 0x4A20A0, 0x4A4610, 0x4A4AB0, 0x4A59C0, 0x4AAB90 and 0x4B0590); openblack's plain sleep, rest to get better, going home to recover and the script's sleep do (`Movement::Kind::FaceDownSlope`, `CreatureMindSystem`; tests in `test/creature/test_creature_rest_and_home.cpp`, `test/test_creature_mind.cpp`), the sit-downs and the sleep by an object do not |

## Routes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land is a grid of 512 by 512 cells sorted once loaded: too steep and deep sea are blocked, shallows can be waded | done | `land_avoid::Validate` (`src/3D/LandAvoid`), built when the landscape opens (`Game.cpp`) as the original's `ValidateLandAvoid` and `FloodAnalyse` ([water](../../../world/water.md#the-creatures-landavoid-mask)): steep cells and sea whose four corners are at altitude 0 are blocked, shallow water can be waded; tests `LandAvoidNearest.*`, `FlatLand.CreaturesWadeTheShallowsButNotTheOpenWater` |
| Land cut off from the main part of the land is out of reach | done | the `land_avoid` flood fill marks it unreachable, as `FloodAnalyse` ([water](../../../world/water.md#the-creatures-landavoid-mask)); test `LandAvoidNearest.OnTheWalkableMask` |
| A creature stands clear of cells it can't walk on | done | `land_avoid::IsPosValid` at radius 7.1 and `NearestValid`, used by `CreatureLocomotionSystem`; tests `LandAvoidNearest.*`, `CreatureLocomotionSystemTest.NowhereToStandIsAnInvalidDestination` |
| Things in the way are walked round as circles; big creatures walk through small trees and over low things | done | our route planner (`src/RoutePlanner`, obstacles from `src/ECS/RoutePlanWorld.cpp`) walks round things as circles, by the original's rule for each kind of thing with the creature's height (`route_plan_world::MustAvoidByRule`): a thing a tenth of its height or taller, a building begun and that tall, a tree one and a half times its height or taller, or any of them burning; a dead or felled tree, a field or a food pile only burning, unless a script controls the creature; tests `RoutePlanWorldCreatureTest.*` |
| Villagers and animals are never walked round; other creatures only while they stand still; anything burning always | partial 70% | `CreatureMustAvoid` (`src/ECS/RoutePlanWorld.cpp`): villagers and animals are never walked round; another creature is not either, standing or not (openblack does not tell its route state yet); anything burning is, by its kind's rule; another player's shield is (test `RoutePlanWorldCreatureTest.AnotherPlayersShieldIsAvoided`) |
| Routes are planned over a finer lattice a budget a turn, straightened where clear, and their corners rounded | done | the original's route planner and follower (`src/RoutePlanner`, `route_planner::RouteFollower`), a few search turns each game turn in `CreatureLocomotionSystem::ProcessTurn`, corners rounded into arcs; tests in `test/test_rplan.cpp`, `test/test_rplan_follow.cpp`, `RouteFollowerCreature.ContextAndRemainingLength`, `CreatureLocomotionSystemTest.AWalkIsPlannedFollowedAndEnded` |
| It goes round cliffs | done | the route planner's square check reads the walkable mask (`CheckSquareFunction` in `src/ECS/RoutePlanWorld.cpp`); tests in `test/creature/test_rplan_world_creature.cpp` |
| With no way there it gives up, puzzled | inferred | `CreatureLocomotionSystem::ProcessTurn`: a failed plan stands it where it is; while it waits for a route it fidgets (a look about, the puzzled face or the confused animation, after 3 s); the fidgets and their timings are raffclar's and not read in the exe |
| It arrives anywhere on a ring about its destination | partial 50% | the route follower's arrival ring (`creature_locomotion::MoveRing`, `ObjectRing`, `Arrived`); test `CreatureLocomotion.ArrivalRings`; the ring's sizes the original passes for a creature are not read ([overview](../../../creature/overview.md#pending)) |
| A walk is given a time to arrive before it is given up | inferred | openblack gives a route's planning 5 s plus 2 ms a unit of distance (`creature_locomotion::TimeLimitMs`, test `CreatureLocomotion.ArrivalRings`), a stand-in: no such limit is read in the exe |
| Stuck or trapped in an enclosed space, it tries to get out (unconfirmed how) | inferred | unconfirmed how; not read in the exe; see [decision_making.md](decision_making.md) |
| It uses the land's footpaths (unconfirmed) | inferred | unconfirmed: whether the original's creature routes take footpaths is not read; the route planner can follow footpaths (`test/test_use_footpath.cpp`), but the creature's walks do not ask for them |
| A villager or animal under its feet can be trodden on | pending | see [object_actions.md](object_actions.md) |

## Going places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Walking or running to a point | done | `CreatureLocomotionSystem::MoveTo`; test `CreatureLocomotionSystemTest.AWalkIsPlannedFollowedAndEnded` |
| Walking up to a thing, stopping short by both their sizes | partial 50% | `CreatureLocomotionSystem::MoveToObject`, the ring from both radii (`creature_locomotion::ObjectRing`, test `CreatureLocomotion.ArrivalRings`); the distances the original passes are not read |
| Following a thing as it moves, keeping within a distance | done | `CreatureLocomotionSystem::Follow` |
| Running away from a point | done | `CreatureLocomotionSystem::FleeFrom`; tests `CreatureLocomotion.RunsAwayFromTheThreat`, `CreatureLocomotionSystemTest.RunningAwayDrawsOnceFromTheSyncedStream` |
| Turning on the spot to face a point | done | `CreatureLocomotionSystem::TurnToFace`; test `CreatureLocomotionSystemTest.TurnToFaceTurnsOnTheSpot` |
| Led on the leash to the hand | done | `CreatureLocomotionSystem::LeadTo`, from `LeashSystem`; see [leash.md](leash.md) |
| Following the hand, or keeping to the middle of the screen | done | the steps that follow the hand and the camera's focus (`Step::Kind::FollowHand`, `FollowCamera`) walk to them again whenever they get away or out of the way it faces, as the exe's handlers 0x505FA0 and 0x503C60 ([mind](../../../creature/mind.md)); test `CreatureMindSystemTest.ANextStepWaitsWhileTheHandsStillActOnAThing` |
| Walking to the beach, up a hill, along a ridge | partial 70% | the exploring actions walk to the places of the map of the land (`CreatureMindSystem::SituateExploration`); the stage gate, the clear area and the quirks (the furthest region, the low corner, the hill sit that does not sit) are read in the exe; the region map itself (64 by 64 regions, the hill rule) is raffclar's and not read ([plans and needs](../../../creature/plans-and-needs.md)); see [idle_behaviour.md](idle_behaviour.md) |
| Travelling by teleport: walking to a stone and jumping to its partner | pending | the teleport miracle moves no creature; see [../miracles](../miracles/) |
| Carried off by a tornado, it faints where it lands | pending | the tornado (`src/Magic/Spells/SpellStormAndTornado.cpp`) takes no creature; see [../miracles](../miracles/) |
| Knocked over by a heavy blow or a falling thing, it falls and gets up again | pending | see [../physics](../physics/) |
| Moved by a script without walking | pending |  |
| Its position is sent to the other players in a network game | pending | see [../multiplayer](../multiplayer/) |

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
| Footprints are burned into the land's block textures, after the snow | pending | openblack draws them in a pass of their own over the land (`FootprintSystem`); the original bakes them into the block textures ([frame](../../../rendering/frame.md)); fork issue #184 |
