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
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "Common/GameAngle.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Events/MapEvents.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WalkGoal.h"
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
using wall_hug::WalkState;

/// The walker as its walk reads and moves it
struct Walker
{
	ecs::Registry& registry;
	entt::entity entity;
	Transform& transform;
	WallHug& wallHug;

	[[nodiscard]] glm::ivec2 Pos() const { return villager::WalkPosition(transform, wallHug); }
	[[nodiscard]] glm::ivec2 Goal() const { return villager::WalkGoal(wallHug); }
	[[nodiscard]] int32_t Speed() const { return WholeSpeed(wallHug); }
	[[nodiscard]] const WallHugObjectReference* Circle() const { return registry.TryGet<const WallHugObjectReference>(entity); }
};

/// The hug of a circle a walker came across, as it keeps it
WallHugObjectReference ReferenceTo(const thing_circles::CellCircle& found)
{
	return {.entity = found.owner,
	        .circle = found.index,
	        .centre = found.circle.centre,
	        .radius = found.circle.radius,
	        .landscape = found.circle.landscape,
	        .cell = found.cell,
	        .landscapeOrFence = found.circle.landscapeOrFence};
}

/// The map cell of a map position: the high words, read unsigned
glm::ivec2 CellOf(glm::ivec2 whole)
{
	return {map_coords::CellOf(whole.x), map_coords::CellOf(whole.y)};
}

/// The circles of the map cell of a map position, in the order the walk comes across them
std::vector<thing_circles::CellCircle> CirclesAt(glm::ivec2 whole)
{
	return thing_circles::CirclesInCell(CellOf(whole));
}

std::vector<wall_hug::BlockingCircle> BlockersOf(const std::vector<thing_circles::CellCircle>& found)
{
	std::vector<wall_hug::BlockingCircle> circles;
	circles.reserve(found.size());
	std::ranges::transform(found, std::back_inserter(circles), &thing_circles::CellCircle::circle);
	return circles;
}

/// The walker's walk state, as a move state tag of the right way round
void SetState(Walker& walker, WalkState state)
{
	auto& registry = walker.registry;
	const auto e = walker.entity;
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(e);
	const auto here = glm::xz(walker.transform.position);
	switch (state)
	{
	case WalkState::Linear:
		registry.Assign<MoveStateLinearTag>(e, MoveStateClockwise::Undefined, here);
		break;
	case WalkState::LinearClockwise:
		registry.Assign<MoveStateLinearTag>(e, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::LinearAnticlockwise:
		registry.Assign<MoveStateLinearTag>(e, MoveStateClockwise::CounterClockwise, here);
		break;
	case WalkState::OrbitClockwise:
		registry.Assign<MoveStateOrbitTag>(e, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::OrbitAnticlockwise:
		registry.Assign<MoveStateOrbitTag>(e, MoveStateClockwise::CounterClockwise, here);
		break;
	case WalkState::ExitClockwise:
		registry.Assign<MoveStateExitCircleTag>(e, MoveStateClockwise::Clockwise, here);
		break;
	case WalkState::ExitAnticlockwise:
		registry.Assign<MoveStateExitCircleTag>(e, MoveStateClockwise::CounterClockwise, here);
		break;
	case WalkState::StepThrough:
		registry.Assign<MoveStateStepThroughTag>(e, MoveStateClockwise::Undefined, here);
		break;
	case WalkState::FinalStep:
	{
		const auto goal = walker.Goal();
		registry.Assign<MoveStateFinalStepTag>(e, MoveStateClockwise::Undefined,
		                                       glm::vec2(map_coords::ToMetres(goal.x), map_coords::ToMetres(goal.y)));
		break;
	}
	case WalkState::Arrived:
	case WalkState::Wander:
		break;
	}
}

/// MoveMapObject: the walker to a map position on the land, at its own height above the land (0 but where a script
/// set one), at the head of the new cell's list only when the cell changes; what is only drawn follows it smoothly
void Place(Walker& walker, glm::ivec2 whole)
{
	const glm::vec2 at {map_coords::ToMetres(whole.x), map_coords::ToMetres(whole.y)};
	const float altitude =
	    walk_goal::StepHeight(Locator::terrainSystem::value().GetHeightAt(at), walk_goal::WalkerAltitudeOf(walker.entity));
	const glm::vec3 before = walker.transform.position;
	map_cells::MoveMapObject(walker.entity, glm::xzy(glm::vec3(at, altitude)));
	ecs::NoteContinuousMove(walker.registry, walker.entity, before);
	walker.wallHug.placedAt = whole;
}

/// Pos = the goal: on it, as its last step or its arrival says it is there
void PlaceAtGoal(Walker& walker)
{
	const auto goal = walker.Goal();
	Place(walker, goal);
	const glm::vec2 at {map_coords::ToMetres(goal.x), map_coords::ToMetres(goal.y)};
	if (auto* finalStep = walker.registry.TryGet<MoveStateFinalStepTag>(walker.entity))
	{
		finalStep->stepGoal = at;
	}
	if (auto* arrived = walker.registry.TryGet<MoveStateArrivedTag>(walker.entity))
	{
		arrived->stepGoal = at;
	}
}

/// InitStepsXZ: the step aimed at the goal from where it is
void InitStepsXZ(Walker& walker)
{
	villager::InitStepsXZ(walker.transform, walker.wallHug);
}

/// The last step to the goal, once it is within a step of it: its step is its position less the goal
void ToFinalStep(Walker& walker)
{
	walker.wallHug.step = walker.Pos() - walker.Goal();
	walker.wallHug.stepMetres = {map_coords::ToMetres(walker.wallHug.step.x), map_coords::ToMetres(walker.wallHug.step.y)};
	SetState(walker, WalkState::FinalStep);
}

/// The straight walk's look ahead: it lets go of its circle, then, of the circles of the map cell of `at`, takes the
/// nearest one its line from where it stands along its step comes into, and the turns to it; none more than 255 turns
/// away
void LinearSweep(Walker& walker, glm::ivec2 at)
{
	walker.registry.Remove<WallHugObjectReference>(walker.entity);
	const auto found = CirclesAt(at);
	const auto circles = BlockersOf(found);
	const auto scan = wall_hug::ScanLine(walker.Pos(), walker.wallHug.step, circles);
	walker.wallHug.turnsToObj = scan.turnsToObstacle;
	if (scan.circle.has_value() && scan.turnsToObstacle != wall_hug::k_NoObstacleInReach)
	{
		walker.registry.Assign<WallHugObjectReference>(walker.entity, ReferenceTo(found[*scan.circle]));
	}
}

/// The look along the circle it goes round, among the circles of the map cell of `at`: it is handed over to another
/// circle, sent straight to its goal, or given the turns it goes round and its new heading
void CircleSweep(Walker& walker, glm::ivec2 at, bool clockwise)
{
	const auto* held = walker.Circle();
	if (held == nullptr)
	{
		// (openblack, guard) the original reads its circle here unchecked: a circle that goes is let go of with its walk
		// changed at once, so a walk round a circle always holds one
		return;
	}
	const auto found = CirclesAt(at);
	const auto circles = BlockersOf(found);
	const auto sweep = wall_hug::SweepCircle({.position = walker.Pos(),
	                                          .goal = walker.Goal(),
	                                          .centre = held->centre,
	                                          .radius = held->radius,
	                                          .clockwise = clockwise,
	                                          .wholeSpeed = walker.Speed(),
	                                          .blockers = circles});
	if (sweep.hugged.has_value())
	{
		walker.registry.AssignOrReplace<WallHugObjectReference>(walker.entity, ReferenceTo(found[*sweep.hugged]));
		walker.wallHug.needsLookahead = sweep.needsLookahead;
	}
	if (sweep.outcome == wall_hug::CircleSweep::Outcome::StepThrough)
	{
		// straight on to its goal inside the circle: the step aimed again in 16 turns, the hug reset
		InitStepsXZ(walker);
		walker.wallHug.turnsUntilStepRebuild = 16;
		wall_hug::ResetCircleHug(walker.registry, walker.entity);
		SetState(walker, WalkState::StepThrough);
		return;
	}
	walker.wallHug.turnsToObj = sweep.turnsToObstacle;
	if (sweep.heading.has_value())
	{
		villager::SetAngleAndStep(walker.transform, walker.wallHug, *sweep.heading);
	}
}

/// Counts the turns to the circle down: true on the turn they were 0 (none, 0xFF, stays as it is)
bool TurnsRunOut(WallHug& wallHug)
{
	if (wallHug.turnsToObj == wall_hug::k_NoObstacleInReach)
	{
		return false;
	}
	return wallHug.turnsToObj-- == 0;
}

/// Whether the walker is within a step of its goal
bool AtGoal(const Walker& walker)
{
	return wall_hug::WithinStep(walker.Pos(), walker.Goal(), walker.Speed());
}

/// The middle of a circle, as a map position
glm::ivec2 CircleCoords(const WallHugObjectReference& circle)
{
	return wall_hug::ToWhole(circle.centre);
}

/// The straight walk: the next position along its step; crossing into another map cell it aims at its goal and looks
/// ahead from there (the step it takes this turn is the one it had); once the turns to its circle run out it starts
/// round it, the way the circle's middle lies from its step (or the way it already went), looks along it and notes
/// how far it is from its goal
void MoveToCircleHug(Walker& walker, WalkState state)
{
	const auto pos = walker.Pos();
	const auto next = pos + walker.wallHug.step;
	if (CellOf(next) != CellOf(pos))
	{
		InitStepsXZ(walker);
		LinearSweep(walker, next);
	}
	if (TurnsRunOut(walker.wallHug))
	{
		if (const auto* held = walker.Circle(); held != nullptr)
		{
			// reaching water or the land's edge (not a fence) makes it a walk to try both ways round
			if (held->landscape)
			{
				walker.wallHug.needsLookahead = true;
			}
			bool clockwise = state == WalkState::LinearClockwise;
			if (state == WalkState::Linear)
			{
				clockwise = wall_hug::GoesRoundClockwise(pos, CircleCoords(*held), walker.wallHug.step);
			}
			SetState(walker, clockwise ? WalkState::OrbitClockwise : WalkState::OrbitAnticlockwise);
			CircleSweep(walker, pos, clockwise);
		}
		walker.wallHug.entryDistance = wall_hug::EntryDistance(pos, walker.Goal());
	}
	Place(walker, next);
}

/// It heads straight on again the same way round, aimed at its goal, and looks ahead from where it stands
void HeadStraightOn(Walker& walker, WalkState state)
{
	InitStepsXZ(walker);
	SetState(walker, state);
	LinearSweep(walker, walker.Pos());
}

/// Straight to the goal, with no look ahead; a villager aims its step again when its counter runs out, then every 127
/// turns (an animal would aim it every turn)
void StepThrough(Walker& walker)
{
	if (--walker.wallHug.turnsUntilStepRebuild == 0)
	{
		walker.wallHug.turnsUntilStepRebuild = 127;
		InitStepsXZ(walker);
	}
	Place(walker, walker.Pos() + walker.wallHug.step);
	if (AtGoal(walker))
	{
		ToFinalStep(walker);
	}
}

/// The straight walk, which looks ahead again from where it stands when it has turns to a circle but holds none
void Linear(Walker& walker, WalkState state)
{
	if (walker.wallHug.turnsToObj != wall_hug::k_NoObstacleInReach && walker.Circle() == nullptr)
	{
		LinearSweep(walker, walker.Pos());
	}
	MoveToCircleHug(walker, state);
	if (AtGoal(walker))
	{
		ToFinalStep(walker);
	}
}

/// Round a circle: a turn by its speed over the radius, then the step along the new heading; crossing into another map
/// cell, and when its turns run out, it looks along the circle again from the next position; nearer its goal than when
/// it started round, with the goal ahead and off to the side away from its turn, it leaves the circle facing out
void Orbit(Walker& walker, bool clockwise)
{
	const auto* held = walker.Circle();
	if (held == nullptr)
	{
		HeadStraightOn(walker, clockwise ? WalkState::LinearClockwise : WalkState::LinearAnticlockwise);
		return;
	}
	const int32_t turn = wall_hug::OrbitTurn(walker.Speed(), held->radius);
	const int32_t angle = clockwise ? walker.wallHug.gameAngle - turn : walker.wallHug.gameAngle + turn;
	villager::SetAngleAndStep(walker.transform, walker.wallHug, static_cast<uint16_t>(angle & game_math::k_GameAngleMask));
	const auto next = walker.Pos() + walker.wallHug.step;
	if (CellOf(next) != CellOf(walker.Pos()))
	{
		CircleSweep(walker, next, clockwise);
	}
	if (TurnsRunOut(walker.wallHug))
	{
		CircleSweep(walker, next, clockwise);
	}
	Place(walker, next);
	if (AtGoal(walker))
	{
		ToFinalStep(walker);
		return;
	}
	const auto* circle = walker.Circle();
	if (circle == nullptr ||
	    !wall_hug::LeavesCircle(walker.Pos(), walker.Goal(), walker.wallHug.step, walker.wallHug.entryDistance, clockwise))
	{
		return;
	}
	villager::SetAngleAndStep(walker.transform, walker.wallHug, game_math::GetAngleFromXZ(CircleCoords(*circle), walker.Pos()));
	SetState(walker, clockwise ? WalkState::ExitClockwise : WalkState::ExitAnticlockwise);
	if (walker.wallHug.entryDistance >= wall_hug::k_LongEntryDistance)
	{
		walker.wallHug.entryDistance = 0;
	}
}

/// Out of the circle, facing away from its middle: once further from it than its radius it heads straight for its goal
void ExitCircle(Walker& walker, bool clockwise)
{
	const auto straightOn = clockwise ? WalkState::LinearClockwise : WalkState::LinearAnticlockwise;
	if (walker.Circle() == nullptr)
	{
		HeadStraightOn(walker, straightOn);
		return;
	}
	Place(walker, walker.Pos() + walker.wallHug.step);
	if (AtGoal(walker))
	{
		ToFinalStep(walker);
		return;
	}
	const auto& circle = *walker.Circle();
	if (circle.radius * circle.radius < wall_hug::MetresDistanceSq(CircleCoords(circle), walker.Pos()))
	{
		HeadStraightOn(walker, straightOn);
	}
}

/// The moment the circle a walker holds goes, as its move state says (wall_hug::OnCircleGone)
void LetGoOfGoneCircle(ecs::Registry& registry, entt::entity e)
{
	const auto state = wall_hug::WalkStateOf(registry, e);
	auto* transform = registry.TryGet<Transform>(e);
	auto* wallHug = registry.TryGet<WallHug>(e);
	if (!state.has_value() || transform == nullptr || wallHug == nullptr)
	{
		registry.Remove<WallHugObjectReference>(e);
		return;
	}
	Walker walker {registry, e, *transform, *wallHug};
	const auto gone = wall_hug::OnCircleGone(*state);
	if (gone.reAim)
	{
		InitStepsXZ(walker);
	}
	if (gone.state != *state)
	{
		SetState(walker, gone.state);
	}
	if (gone.lookAgain)
	{
		// from where it stands, along the step it has now
		LinearSweep(walker, walker.Pos());
	}
	if (gone.letGo)
	{
		registry.Remove<WallHugObjectReference>(e);
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
	// the walkers going round a circle of a thing that overlaps one of many cells that leaves leave it at their next
	// chance
	if (!overlapping.empty())
	{
		registry.Each<const WallHugObjectReference, WallHug>(
		    [overlapping](entt::entity, const WallHugObjectReference& reference, WallHug& wallHug) {
			    if (!reference.landscape && std::ranges::find(overlapping, reference.entity) != overlapping.end())
			    {
				    wallHug.entryDistance = wall_hug::k_LeaveAtOnce;
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
	auto* transform = registry.TryGet<Transform>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	if (transform == nullptr || wallHug == nullptr)
	{
		return;
	}
	// the circles of the cell it stands in, as the original's set-up sweeps from its position
	Walker walker {registry, entity, *transform, *wallHug};
	LinearSweep(walker, walker.Pos());
}

void PathfindingSystem::Step(entt::entity entity)
{
	// MoveTo for one walker, called by the state functions that walk (living_turn::MoveToStep): one handler a move state,
	// in whole map units
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || registry.AllOf<Unavailable>(entity))
	{
		return;
	}
	auto* transform = registry.TryGet<Transform>(entity);
	auto* wallHug = registry.TryGet<WallHug>(entity);
	const auto state = wall_hug::WalkStateOf(registry, entity);
	if (transform == nullptr || wallHug == nullptr || !state.has_value())
	{
		return;
	}
	Walker walker {registry, entity, *transform, *wallHug};
	// no hand-over to water, the land's edge or a fence yet this turn
	wallHug->needsLookahead = false;
	switch (*state)
	{
	case WalkState::FinalStep:
		PlaceAtGoal(walker);
		break;
	case WalkState::Arrived:
		if (AtGoal(walker))
		{
			PlaceAtGoal(walker);
			break;
		}
		walker.wallHug.turnsUntilStepRebuild = 16;
		SetState(walker, WalkState::StepThrough);
		StepThrough(walker);
		break;
	case WalkState::StepThrough:
		StepThrough(walker);
		break;
	case WalkState::Linear:
	case WalkState::LinearClockwise:
	case WalkState::LinearAnticlockwise:
		Linear(walker, *state);
		break;
	case WalkState::OrbitClockwise:
		Orbit(walker, true);
		break;
	case WalkState::OrbitAnticlockwise:
		Orbit(walker, false);
		break;
	case WalkState::ExitClockwise:
		ExitCircle(walker, true);
		break;
	case WalkState::ExitAnticlockwise:
		ExitCircle(walker, false);
		break;
	case WalkState::Wander:
		break;
	}
}
