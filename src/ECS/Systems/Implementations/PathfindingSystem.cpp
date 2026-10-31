/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PathfindingSystem.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Events/MapEvents.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/WalkStateOf.h"
#include "ECS/WallHugBookkeeping.h"
#include "ECS/WallHugRules.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// The heading and step of openblack's own orbit (InitializeStepAroundObstacle, IterateStepAroundObstacle, the exit of
/// the circle). (approximate) not the original's circle hug and its sweeps: float radians and metres, left to the
/// hug's port
void InitializeStep(Transform& transform, WallHug& wallHug, float angle)
{
	openblack::ecs::villager::SetYAngle(transform, wallHug, angle);
	wallHug.stepMetres = glm::vec2(glm::cos(angle), glm::sin(angle)) * wallHug.speed;
}

/// villager::InitStepsXZ: the game angle at the goal, the integer step
void InitializeStepToGoal(Transform& transform, WallHug& wallHug)
{
	openblack::ecs::villager::InitStepsXZ(transform, wallHug);
}

void InitializeStepAroundObstacle(Transform& transform, WallHug& wallHug, glm::vec2 centre, float numCirclesAway,
                                  bool clockwise)
{
	// Use doubles for atan to have same precision on linux, osx and win32
	const glm::dvec2 diff = centre - glm::xz(transform.position);
	const auto angle = static_cast<float>(glm::atan(diff.y, diff.x));

	const float clockwiseModifier = clockwise ? 1.0f : -1.0f;
	const float angleStep = glm::radians(90.0f) - glm::radians(90.0f / 8.0f) * numCirclesAway;
	InitializeStep(transform, wallHug, angle + angleStep * clockwiseModifier);
}

/// The hug of a circle a walker came across, as it keeps it
WallHugObjectReference ReferenceTo(uint8_t stepsAway, const thing_circles::CellCircle& found)
{
	return {.stepsAway = stepsAway,
	        .entity = found.owner,
	        .circle = found.index,
	        .centre = found.circle.centre,
	        .radius = found.circle.radius,
	        .landscape = found.circle.landscape,
	        .cell = found.cell,
	        .landscapeOrFence = found.circle.landscapeOrFence};
}

/// Whether a circle a walker came across is the one it holds: the same circle of the same thing, or the circle of the
/// same cell of water or off the land
bool IsHeldCircle(const WallHugObjectReference& reference, const thing_circles::CellCircle& found)
{
	if (reference.landscape || found.circle.landscape)
	{
		return reference.landscape && found.circle.landscape && reference.cell == found.cell;
	}
	return reference.entity == found.owner && reference.circle == found.index;
}

/// Whether the walker holds a circle: a thing's, or one of water or the land's edge
bool HoldsCircle(const WallHugObjectReference& reference)
{
	return reference.landscape || reference.entity != entt::null;
}

/// A walker going round a circle of a thing that overlaps one of many cells leaving the map may leave its circle at its
/// next chance: the original's entry distance as far as it goes
constexpr uint32_t k_LeaveAtOnce = 0x7FFFFFFF;

void IterateStepAroundObstacle(Transform& transform, WallHug& wallHug, float radius, bool clockwise)
{
	const float clockwiseModifier = clockwise ? 1.0f : -1.0f;
	const float angle = wallHug.yAngle;
	const float angleStep = -wallHug.speed / radius;
	InitializeStep(transform, wallHug, angle + angleStep * clockwiseModifier);
}

/// The wall-hug/orbit movement port has unfinished branches (the goal inside the hugged circle, TODO #864, and the
/// circle-to-circle handover, TODO #865: the circle hug's square sweeps in the original).
/// The original never gives a walk up: its MoveTo returns 0 / 1 / 6 / 7 while it walks and 0xA only when it is there
/// (ARRIVED with AreWeThere, FINAL_STEP), and the Living's move to a position changes state only on 0xA; there is no
/// "abandoned" code. (approximate) So instead of dropping the walk (which the villagers took as an arrival where
/// they stood), the unported branch goes on as STEP_THROUGH (0xB: the straight walk to the goal, InitStepsXZ, no
/// obstacle handling): MoveTo keeps walking and the villager arrives only through AreWeThere (step 5 -> FINAL_STEP).
/// The step is aimed again in 16 turns, as when the original's look along a circle sends a walker straight to a goal
/// inside the circle (approximate: that branch is not yet ported, and the other unported branches have no original)
void AbandonMove(ecs::Registry& registry, entt::entity entity)
{
	SPDLOG_LOGGER_WARN(spdlog::get("pathfinding"),
	                   "Villager #{}: unimplemented pathfinding case hit, going on straight to the goal (STEP_THROUGH)",
	                   static_cast<uint32_t>(entity));
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag, WallHugObjectReference>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	if (transform == nullptr || wallHug == nullptr)
	{
		return;
	}
	InitializeStepToGoal(*transform, *wallHug);
	wallHug->turnsUntilStepRebuild = 16;
	registry.Assign<MoveStateStepThroughTag>(entity, MoveStateClockwise::Undefined, glm::xz(transform->position));
}
bool AreWeThere(const glm::vec2& pos, const glm::vec2& goal, float threshold)
{
	return glm::distance2(pos, goal) < threshold * threshold;
}

/// The circles a walker comes across in the map cell of a point (metres): the cell of its MapCoords
std::vector<thing_circles::CellCircle> CirclesAt(const glm::vec2& point)
{
	return thing_circles::CirclesInCell(map_coords::CellOf(point));
}

/// The straight line's look ahead, as the original's: of the circles of the map cell of sweepAt, the nearest one the
/// walker's line from pos along its step comes into (one it is more than a fifth of a metre inside does not count),
/// and the turns to it; none more than 255 turns away
bool LinearScanForObstacle(entt::entity entity, const glm::vec2& pos, const glm::vec2& step, const glm::vec2& sweepAt)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Reference will be updated or removed
	registry.Remove<WallHugObjectReference>(entity);

	const auto found = CirclesAt(sweepAt);
	std::vector<wall_hug::BlockingCircle> circles;
	circles.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(circles), &thing_circles::CellCircle::circle);
	const auto scan = wall_hug::ScanLineFrom(pos, step, circles);
	if (!scan.circle.has_value())
	{
		return false;
	}

	// Store the circle and the number of steps to it
	registry.Assign<WallHugObjectReference>(entity, ReferenceTo(scan.turnsToObstacle, found[*scan.circle]));
	return true;
}

/// If we have a number of turns to an obstacle but no obstacle saved: search in an arc to find one, among the circles
/// of the map cell of sweepAt
bool OrbitScanForObstacle(entt::entity entity, bool clockwise, Transform& transform, WallHug& wallHug, const glm::vec2& sweepAt)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& reference = registry.Get<WallHugObjectReference>(entity);
	const auto circles = CirclesAt(sweepAt);

	const uint32_t numAttempts = 5;
	bool found = false;
	for (uint32_t attempt = 0; attempt < numAttempts; ++attempt)
	{
		const glm::vec2 hugCentre = reference.centre;
		const float hugRadius = reference.radius;
		const auto circleToVillager = glm::xz(transform.position) - hugCentre;
		const float circleToVillagerLength = glm::length(circleToVillager);
		const float numCirclesAway = circleToVillagerLength / hugRadius - 0.9f;
		const auto circleNormal = circleToVillager / circleToVillagerLength;

		const bool closeEnough = glm::distance2(wallHug.goal, hugCentre) < hugRadius * hugRadius;
		if (closeEnough)
		{
			// TODO(#864): CircleSquareSweep is unimplemented. Degrade gracefully instead of
			// crashing, returning out of this scope so the external following code does not run.
			AbandonMove(registry, entity);
			return false;
		}

		if (!circles.empty())
		{
			// the first other circle of the cell that overlaps the held one
			auto iter = std::ranges::find_if(circles, [&reference, hugCentre, hugRadius](const auto& c) {
				if (IsHeldCircle(reference, c))
				{
					return false;
				}
				const auto d2 = glm::distance2(c.circle.centre, hugCentre);
				const auto r = c.circle.radius + hugRadius;
				const auto r2 = r * r;
				return d2 < r2 && d2 > 0.0f;
			});
			if (iter != circles.end())
			{
				// https://stackoverflow.com/questions/3349125/circle-circle-intersection-points
				// http://paulbourke.net/geometry/circlesphere/
				const auto d2 = glm::distance2(iter->circle.centre, hugCentre);
				const auto d = glm::sqrt(d2);

				// both radii as the collide data has them (the circles are already the size of their things)
				const auto r0 = iter->circle.radius;
				const auto r1 = hugRadius;

				const auto r02 = r0 * r0;
				const auto r12 = r1 * r1;
				const auto p0 = iter->circle.centre;
				const auto p1 = hugCentre;
				const auto a = (r02 - r12 + d2) / (2.0f * d); // first circle to intersection midpoint
				const auto h = glm::sqrt(r02 - a * a);        // half height of intersection area
				const auto p2 = p0 + a * (p1 - p0) / d;       // midpoint of overlap
				const auto diff = p1 - p0;
				const auto difft = glm::vec2(diff.y, -diff.x); // 90 degree rotation
				const auto p3 = p2 + h * difft / d;
				const auto p4 = p2 - h * difft / d;

				const auto v0 = p3 - hugCentre;
				const auto v1 = p4 - hugCentre;
				const auto n0 = glm::normalize(v0);
				const auto n1 = glm::normalize(v1);
				const auto dp0 = glm::dot(n0, circleNormal);
				const auto dp1 = glm::dot(n1, circleNormal);
				const auto cp0 = glm::cross(glm::vec3(n0, 0.0f), glm::vec3(circleNormal, 0.0f)).z;
				const auto cp1 = glm::cross(glm::vec3(n1, 0.0f), glm::vec3(circleNormal, 0.0f)).z;
				auto angle0 = glm::acos(dp0);
				auto angle1 = glm::acos(dp1);

				if ((cp0 > 0.0f) ^ clockwise)
				{
					angle0 = 2.0f * glm::pi<float>() - angle0;
				}
				if ((cp1 > 0.0f) ^ clockwise)
				{
					angle1 = 2.0f * glm::pi<float>() - angle1;
				}

				const auto t0 = angle0 * 2.0f / 3.0f * r1 / wallHug.speed;
				const auto t1 = angle1 * 2.0f / 3.0f * r1 / wallHug.speed;
				int t = static_cast<int>(glm::round(glm::min(t0, t1)));

				if (t < 0)
				{
					// Unfinished orbit geometry produced a negative step count. Degrade gracefully
					// (bail out of the scan) rather than asserting and crashing.
					AbandonMove(registry, entity);
					return false;
				}
				if (t < 1)
				{
					// We're too close to second circle. Act like we're on the second circle and continue looking forward by
					// recursively calling function with new obstacle. Handed over to water, the land's edge or a fence, the
					// walk is one to try both ways round; handed over again, the last circle decides
					reference = ReferenceTo(reference.stepsAway, *iter);
					wallHug.needsLookahead = iter->circle.landscapeOrFence;
					found = false; // will do another loop
				}
				else if (t < 4)
				{
					t = 0;
					found = true;
				}
				else
				{
					t = glm::min(t, 255);
					found = true;
				}

				reference.stepsAway = static_cast<uint8_t>(t);
			}
			else
			{
				// TODO(bwrsandman):
				// if intersect[0].obj is None:  # True
				//     self.init_steps_xz()
				//     # self.field_0x78 = 0x10
				//     self.circle_hug_info.reset(self)
				//     self.move_state = MoveState.STEP_THROUGH
				// assert(false);
				found = true;
			}
		}
		if (found)
		{
			InitializeStepAroundObstacle(transform, wallHug, hugCentre, numCirclesAway, clockwise);
			break;
		}
	}

	return found;
}

template <typename... X>
bool Excluded(const ecs::Registry& registry, entt::entity entity, entt::exclude_t<X...> /*exclude*/)
{
	if constexpr (sizeof...(X) == 0)
	{
		return false;
	}
	else
	{
		return registry.AnyOf<X...>(entity);
	}
}

/// registry.Each<C...>(func, entt::exclude<X...>) for one entity: func(entity, its C...) when it has every C and none of
/// the X. The walk of one villager goes through the same phases, in the same order, as the walk of everyone did
template <typename... C, typename... Exclude, typename Func>
void ForOne(ecs::Registry& registry, entt::entity entity, Func func, Exclude... exclude)
{
	if (!registry.AllOf<std::remove_const_t<C>...>(entity) || (Excluded(registry, entity, exclude) || ...))
	{
		return;
	}
	func(entity, registry.Get<std::remove_const_t<C>>(entity)...);
}

template <MoveState S>
void StepForward(ecs::Registry& registry, entt::entity entity)
{
	ForOne<MoveStateTagComponent<S>, const WallHug, Transform>(
	    registry, entity,
	    [](entt::entity, MoveStateTagComponent<S>& state, const WallHug& wallHug, const Transform& transform) {
		    const auto goal = glm::xz(transform.position) + wallHug.stepMetres;
		    state.stepGoal = goal;
	    },
	    entt::exclude<Unavailable>);
}

template <MoveState S>
bool CellTransition(entt::entity entity, const MoveStateTagComponent<S>& state, Transform& transform, WallHug& wallHug);

template <>
bool CellTransition(entt::entity entity, const MoveStateTagComponent<MoveState::Linear>& state, Transform& transform,
                    WallHug& wallHug)
{
	// the next position's cell (the step taken before this aim), looked along from where it stands
	InitializeStepToGoal(transform, wallHug);
	return LinearScanForObstacle(entity, glm::xz(transform.position), wallHug.stepMetres, state.stepGoal);
}

template <>
bool CellTransition(entt::entity entity, const MoveStateTagComponent<MoveState::Orbit>& state, Transform& transform,
                    WallHug& wallHug)
{
	return OrbitScanForObstacle(entity, state.clockwise == MoveStateClockwise::Clockwise, transform, wallHug, state.stepGoal);
}

/// Transition from one grid cell to another requires another check for obstacle in the line
template <MoveState S>
void HandleCellTransition(ecs::Registry& registry, entt::entity entity)
{
	ForOne<const MoveStateTagComponent<S>, WallHug, Transform>(
	    registry, entity,
	    [](entt::entity e, const MoveStateTagComponent<S>& state, WallHug& wallHug, Transform& transform) {
		    const auto position = glm::xz(transform.position);
		    const auto positionId = MapInterface::GetGridCell(position);
		    const auto goalId = MapInterface::GetGridCell(state.stepGoal);
		    if (positionId != goalId)
		    {
			    CellTransition(e, state, transform, wallHug);
		    }
	    },
	    entt::exclude<Unavailable>);
}

// TODO(bwrsandman): Vanilla is more complex than this. Update to the map might be needed when transitioning from one block to
// the other.
template <MoveState S>
void ApplyStepGoal(ecs::Registry& registry, entt::entity entity)
{
	ForOne<const MoveStateTagComponent<S>, Transform>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateTagComponent<S>& state, Transform& transform) {
		    const float altitude = Locator::terrainSystem::value().GetHeightAt(state.stepGoal);
		    const glm::vec3 before = transform.position;
		    // map_cells::MoveMapObject: the new position, and the head of the new cell's list only when the cell changes
		    // (map_cells writes the Transform)
		    map_cells::MoveMapObject(e, glm::xzy(glm::vec3(state.stepGoal, altitude)));
		    // one of its own steps: what is only drawn follows it smoothly (ECS/MobileDrawing.h)
		    ecs::NoteContinuousMove(registry, e, before);
	    },
	    entt::exclude<Unavailable>);
}

/// A walker whose circle went heads straight on again, the same way round (LINEAR_CW / LINEAR_CCW), from where it
/// stands
void HeadStraightOn(ecs::Registry& registry, entt::entity walker, wall_hug::WalkState state, const Transform& transform)
{
	auto clockwise = MoveStateClockwise::Undefined;
	if (state == wall_hug::WalkState::LinearClockwise)
	{
		clockwise = MoveStateClockwise::Clockwise;
	}
	else if (state == wall_hug::WalkState::LinearAnticlockwise)
	{
		clockwise = MoveStateClockwise::CounterClockwise;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag>(walker);
	registry.Assign<MoveStateLinearTag>(walker, clockwise, glm::xz(transform.position));
}

/// The moment the circle a walker holds goes, as its move state says (wall_hug::OnCircleGone)
void LetGoOfGoneCircle(ecs::Registry& registry, entt::entity walker)
{
	const auto state = wall_hug::WalkStateOf(registry, walker);
	auto* transform = registry.TryGet<Transform>(walker);
	auto* wallHug = registry.TryGet<WallHug>(walker);
	if (!state.has_value() || transform == nullptr || wallHug == nullptr)
	{
		registry.Remove<WallHugObjectReference>(walker);
		return;
	}
	const auto gone = wall_hug::OnCircleGone(*state);
	if (gone.reAim)
	{
		InitializeStepToGoal(*transform, *wallHug);
	}
	if (gone.state != *state)
	{
		HeadStraightOn(registry, walker, gone.state, *transform);
	}
	if (gone.lookAgain)
	{
		// from where it stands, along the step it has now
		const auto position = glm::xz(transform->position);
		LinearScanForObstacle(walker, position, wallHug->stepMetres, position);
	}
	if (gone.letGo)
	{
		registry.Remove<WallHugObjectReference>(walker);
	}
}

} // namespace

void openblack::ecs::systems::AddPathfindingEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::CollideDataReleased>([](const events::CollideDataReleased& event) {
		if (Locator::pathfindingSystem::has_value())
		{
			Locator::pathfindingSystem::value().OnCollideDataReleased(event.thing, event.overlapping);
		}
	});
}

void PathfindingSystem::OnCollideDataReleased(entt::entity thing, std::span<const entt::entity> overlapping)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the walkers going round a circle of a thing that overlaps one of many cells that leaves may leave it at their
	// next chance (openblack's float orbit has no such test yet, and leaves as if this were always so)
	if (!overlapping.empty())
	{
		registry.Each<WallHugObjectReference>([overlapping](entt::entity, WallHugObjectReference& reference) {
			if (!reference.landscape && std::ranges::find(overlapping, reference.entity) != overlapping.end())
			{
				reference.entryDistance = k_LeaveAtOnce;
			}
		});
	}
	// then every walker holding one of the thing's circles, at once
	std::vector<entt::entity> walkers;
	registry.Each<const WallHugObjectReference>(
	    [thing, &walkers](entt::entity walker, const WallHugObjectReference& reference) {
		    if (!reference.landscape && reference.entity == thing)
		    {
			    walkers.push_back(walker);
		    }
	    });
	for (const auto walker : walkers)
	{
		LetGoOfGoneCircle(registry, walker);
	}
}

void PathfindingSystem::SweepAhead(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* wallHug = registry.TryGet<const WallHug>(entity);
	if (transform == nullptr || wallHug == nullptr)
	{
		return;
	}
	// the circles of the cell it stands in, as the original's set-up sweeps from its position
	const auto position = glm::xz(transform->position);
	LinearScanForObstacle(entity, position, wallHug->stepMetres, position);
}

void PathfindingSystem::Step(entt::entity entity)
{
	// MoveTo for one villager, called by the state functions that walk (living_turn::MoveToStep): the move to a
	// position, the move on a footpath, ... The phases below are openblack's walk (not yet the original's jump table
	// on the move state), each one for this villager only: an obstacle scan sees the positions the livings before it
	// in the list took this turn, as in the original
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	// no hand-over to water, the land's edge or a fence yet this turn
	if (auto* wallHug = registry.TryGet<WallHug>(entity))
	{
		wallHug->needsLookahead = false;
	}

	// 0.  (none: a circle whose thing leaves the map is let go of there and then, OnCollideDataReleased; until then
	//         the walker goes on with the circle as it found it)

	// 1.  ARRIVED:
	//         If AreWeThere is false, set to STEP_THROUGH (and it will trigger following steps), with the step due to
	//         be aimed again in 16 turns. The original's ARRIVED: AreWeThere(0) -> Pos = goal, 0xA; else STEP_THROUGH.
	//         (The test here was inverted: it left ARRIVED precisely when there.)
	ForOne<const MoveStateArrivedTag, const Transform, WallHug>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateArrivedTag& state, const Transform& transform, WallHug& wallHug) {
		    if (!AreWeThere(glm::xz(transform.position), wallHug.goal, wallHug.speed))
		    {
			    wallHug.turnsUntilStepRebuild = 16;
			    registry.SwapComponents<MoveStateStepThroughTag>(e, state, state.clockwise);
		    }
	    },
	    entt::exclude<Unavailable>);

	// 2.  (none: a LINEAR walk is aimed and looks ahead when it is set up, villager::SetupMobileMoveToPos)

	// 3.  ORBIT_CW, ORBIT_CCW, EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW with no circle held (let go of while the walk was left,
	//         for one): the step is aimed at the goal again, the walk heads straight on the same way round (LINEAR_CW /
	//         LINEAR_CCW) and looks ahead from where it stands, and that is all for this turn: no step is taken
	if (const auto state = wall_hug::WalkStateOf(registry, entity); state.has_value() && !registry.AllOf<Unavailable>(entity))
	{
		using wall_hug::WalkState;
		const bool goesRound = *state == WalkState::OrbitClockwise || *state == WalkState::OrbitAnticlockwise ||
		                       *state == WalkState::ExitClockwise || *state == WalkState::ExitAnticlockwise;
		const auto* reference = registry.TryGet<const WallHugObjectReference>(entity);
		if (goesRound && (reference == nullptr || !HoldsCircle(*reference)))
		{
			auto* transform = registry.TryGet<Transform>(entity);
			auto* wallHug = registry.TryGet<WallHug>(entity);
			if (transform == nullptr || wallHug == nullptr)
			{
				// (openblack, guard) a walker with no position or walk (only in the unit tests)
				AbandonMove(registry, entity);
				return;
			}
			InitializeStepToGoal(*transform, *wallHug);
			HeadStraightOn(registry, entity, wall_hug::OnCircleGone(*state).state, *transform);
			const auto position = glm::xz(transform->position);
			LinearScanForObstacle(entity, position, wallHug->stepMetres, position);
			return;
		}
	}

	// 4a. STEP_THROUGH, EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW, LINEAR without obstacles:
	//         Do StepForward and ApplyStepGoal for the step distance -> no change to state. A villager's STEP_THROUGH
	//         first counts its step-rebuild counter down and, when it runs out, aims the step at the goal again and
	//         waits 127 turns for the next aim (an animal would aim every turn; animals do not walk here)
	ForOne<const MoveStateStepThroughTag, Transform, WallHug>(
	    registry, entity,
	    [](entt::entity, const MoveStateStepThroughTag&, Transform& transform, WallHug& wallHug) {
		    if (--wallHug.turnsUntilStepRebuild == 0)
		    {
			    wallHug.turnsUntilStepRebuild = 127;
			    InitializeStepToGoal(transform, wallHug);
		    }
	    },
	    entt::exclude<Unavailable>);
	StepForward<MoveState::StepThrough>(registry, entity);
	StepForward<MoveState::ExitCircle>(registry, entity);
	ApplyStepGoal<MoveState::StepThrough>(registry, entity);
	ApplyStepGoal<MoveState::ExitCircle>(registry, entity);

	// 4b. FINAL_STEP, ARRIVED:
	//         Do ApplyStepGoal for the remaining distance to the goal and return a message to change LIVING STATE
	//         exclude from next parts -> no change to state
	ApplyStepGoal<MoveState::FinalStep>(registry, entity);
	ApplyStepGoal<MoveState::Arrived>(registry, entity);

	// 4c. ORBIT_CW, ORBIT_CCW:
	ForOne<const MoveStateOrbitTag, const WallHugObjectReference, WallHug, Transform>(
	    registry, entity,
	    [](entt::entity, const MoveStateOrbitTag& state, const WallHugObjectReference& reference, WallHug& wallHug,
	       Transform& transform) {
		    IterateStepAroundObstacle(transform, wallHug, reference.radius, state.clockwise == MoveStateClockwise::Clockwise);
	    },
	    entt::exclude<Unavailable>);
	StepForward<MoveState::Orbit>(registry, entity);
	HandleCellTransition<MoveState::Orbit>(registry, entity);
	// Decrement turns to object, remove reference once at 0, 0xFF means there is obstacle
	// TODO(#500): split WallHugObjectReference into FutureObstacle and HuggedObstacle
	ForOne<const MoveStateOrbitTag, WallHugObjectReference>(
	    registry, entity,
	    [](entt::entity, const MoveStateOrbitTag&, WallHugObjectReference& reference) {
		    if (reference.stepsAway == std::numeric_limits<decltype(reference.stepsAway)>::max())
		    {
			    return;
		    }
		    if (reference.stepsAway == 0)
		    {
			    reference.stepsAway = std::numeric_limits<decltype(reference.stepsAway)>::max();
		    }
		    else
		    {
			    --reference.stepsAway;
		    }
	    },
	    entt::exclude<Unavailable>);
	// Call OrbitScanForObstacle for those without reference, jumping from one circle to the next
	ForOne<const MoveStateOrbitTag>(
	    registry, entity,
	    [&registry](entt::entity e, [[maybe_unused]] const MoveStateOrbitTag& state) {
		    if (!registry.AnyOf<WallHugObjectReference>(e))
		    {
			    // TODO(#865): circle-to-circle transition is unimplemented. Degrade gracefully.
			    AbandonMove(registry, e);
		    }
	    },
	    entt::exclude<Unavailable>);
	ApplyStepGoal<MoveState::Orbit>(registry, entity);
	// Check if it's time to exit circle hug
	ForOne<const MoveStateOrbitTag, WallHug, Transform, WallHugObjectReference>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateOrbitTag& state, WallHug& wallHug, Transform& transform,
	                WallHugObjectReference& reference) {
		    const auto pos = glm::xz(transform.position);
		    if (AreWeThere(pos, wallHug.goal, 0.0f))
		    {
			    registry.SwapComponents<MoveStateFinalStepTag>(e, state, MoveStateClockwise::Undefined, wallHug.goal);
			    registry.Remove<WallHugObjectReference>(e);
		    }

		    const auto diff = pos - wallHug.goal;
		    // If continuing around the circle gets us closer, keep hugging
		    // 2D cross product gives the sin between both vectors
		    const float sin = glm::cross(glm::vec3(wallHug.stepMetres, 0.0f), glm::vec3(diff, 0.0f)).z;
		    const auto newClockwise = sin > 0.0f ? MoveStateClockwise::Clockwise : MoveStateClockwise::CounterClockwise;
		    assert(state.clockwise != MoveStateClockwise::Undefined);
		    if (state.clockwise == newClockwise)
		    {
			    return;
		    }
		    // If the angle isn't larger than 90 degrees, keep hugging
		    if (glm::dot(wallHug.stepMetres, diff) > 0.0f)
		    {
			    return;
		    }

		    const auto normal = pos - reference.centre;
		    InitializeStep(transform, wallHug, glm::atan(normal.y, normal.x));
		    // Add exit tag, current tag stay to avoid 6. and is removed after
		    registry.Assign<MoveStateExitCircleTag>(e, state.clockwise, state.stepGoal);
	    },
	    entt::exclude<Unavailable>);

	// 4d. LINEAR, LINEAR_CW, LINEAR_CCW:
	//         Do move_to_circle_hug (complex) -> can change state to ORBIT*
	StepForward<MoveState::Linear>(registry, entity);
	HandleCellTransition<MoveState::Linear>(registry, entity);
	// Decrement turns to object, transition to orbit at 0
	ForOne<const MoveStateLinearTag, Transform, WallHug, WallHugObjectReference>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateLinearTag& state, Transform& transform, WallHug& wallHug,
	                WallHugObjectReference& reference) {
		    assert(reference.stepsAway != 0xFF); // In this case, the component should have been removed
		    if (reference.stepsAway == 0)
		    {
			    auto clockwise = state.clockwise;
			    if (clockwise == MoveStateClockwise::Undefined)
			    {
				    const auto diff = glm::xz(transform.position) - reference.centre;
				    // 2D cross product gives the sin between both vectors
				    const float sin = glm::cross(glm::vec3(wallHug.stepMetres, 0.0f), glm::vec3(diff, 0.0f)).z;
				    // Positive is 180 degrees clockwise, negative is 180 degrees counter-clockwise
				    clockwise = sin > 0.0f ? MoveStateClockwise::Clockwise : MoveStateClockwise::CounterClockwise;
			    }
			    // reaching water or the land's edge (not a fence) makes it a walk to try both ways round
			    if (reference.landscape)
			    {
				    wallHug.needsLookahead = true;
			    }
			    // Add orbit, remove linear later
			    auto& newState = registry.Assign<MoveStateOrbitTag>(e, clockwise, state.stepGoal);
			    reference.stepsAway = std::numeric_limits<decltype(reference.stepsAway)>::max(); // FIXME: useless value
			    // TODO(#500): reference.entity should probably be put in another component
			    // registry.Remove<WallHugObjectReference>(entity);
			    // registry.Remove<MoveStateLinearTag>(entity); // TODO(#500): Maybe do this later

			    // TODO(bwrsandman): perhaps move this to another Each call
			    OrbitScanForObstacle(e, newState.clockwise == MoveStateClockwise::Clockwise, transform, wallHug,
			                         glm::xz(transform.position));
		    }
		    else
		    {
			    --reference.stepsAway;
		    }
	    },
	    entt::exclude<Unavailable>);

	ApplyStepGoal<MoveState::Linear>(registry, entity);
	// Clean-up: Remove those which have been transitioned
	ForOne<const MoveStateLinearTag, const MoveStateOrbitTag>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateLinearTag&, const MoveStateOrbitTag&) {
		    registry.Remove<MoveStateLinearTag>(e);
	    },
	    entt::exclude<Unavailable>);

	// 5.  NOT(FINAL_STEP, ARRIVED): ** PRIOR TO ANY CHANGE OF THE ABOVE STEPS (4c):
	//         if AreWeThere(): sets to FINAL_STEP
	ForOne<WallHug, const Transform>(
	    registry, entity,
	    [&registry](entt::entity e, WallHug& wallHug, const Transform& transform) {
		    if (AreWeThere(glm::xz(transform.position), wallHug.goal, wallHug.speed))
		    {
			    registry.Assign<MoveStateFinalStepTag>(e, MoveStateClockwise::Undefined, wallHug.goal);
			    registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(e);
		    }
	    },
	    entt::exclude<MoveStateFinalStepTag, MoveStateArrivedTag, Unavailable>);

	// 6.  EXIT_CIRCLE_CW, EXIT_CIRCLE_CCW ** PRIOR TO ANY CHANGE OF THE ABOVE STEPS (4c):
	//         if the distance to obstacle is greater than the radius of the circle: set to LINEAR_(C)CW and do
	//         linear_square_sweep
	ForOne<const MoveStateExitCircleTag, WallHug, const WallHugObjectReference, Transform>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateExitCircleTag& state, WallHug& wallHug, const WallHugObjectReference& object,
	                Transform& transform) {
		    if (HoldsCircle(object) && !registry.AnyOf<MoveStateOrbitTag>(e))
		    {
			    const auto position = glm::xz(transform.position);
			    const auto centre = object.centre;
			    const auto radius = object.radius;
			    if (!AreWeThere(position, centre, wallHug.speed))
			    {
				    if (!AreWeThere(position, centre, radius))
				    {
					    InitializeStepToGoal(transform, wallHug);
					    registry.SwapComponents<MoveStateLinearTag>(e, state, state.clockwise, state.stepGoal);
					    LinearScanForObstacle(e, position, wallHug.stepMetres, position);
				    }
			    }
		    }
	    },
	    entt::exclude<Unavailable>);

	// Remove leftover tag from orbit to exit circle transition
	ForOne<const MoveStateExitCircleTag, const MoveStateOrbitTag>(
	    registry, entity,
	    [&registry](entt::entity e, const MoveStateExitCircleTag&, const MoveStateOrbitTag&) {
		    registry.Remove<MoveStateOrbitTag>(e);
	    },
	    entt::exclude<Unavailable>);
}
