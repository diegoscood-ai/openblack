/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>

#include "Common/RandomNumberManager.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

/// Fleeing from predators like the original (docs/bw1-notes/animals.md; research dev\tmp_dis\animals\flee.md): every
/// predator makes reaction 28 at its construction and spreads it once to the animals then within 25 m. The per-turn
/// re-spreading (Reaction::ProcessReactions) is behind a debug flag the shipped game never sets, so grazers rarely flee.
/// openblack's villagers don't take reactions yet.
namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Transform;

namespace
{
constexpr size_t k_FleeFromPredator = 28;

const ReactionInfo& Reaction()
{
	return Locator::infoConstants::value().reaction.at(k_FleeFromPredator);
}

bool IsPredator(AnimalInfo type)
{
	return detail::HunterOf(type) != detail::Hunter::None;
}

/// the predator's speed (u16): speedDefault until it has taken a turn (Animal::Animal sets it)
uint32_t SpeedOf(entt::entity predator)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* brain = registry.TryGet<const AnimalBrain>(predator); brain != nullptr)
	{
		return brain->speed;
	}
	return static_cast<uint32_t>(detail::InfoOf(registry.Get<const Animal>(predator)).speedGroup.speedDefault);
}

/// its movement direction per turn (GetMovementDirection, vt+0x168): the step while it moves
glm::vec2 MovementOf(entt::entity predator)
{
	const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(predator);
	if (brain == nullptr || brain->movedLastTurn <= 0.0f)
	{
		return glm::vec2(0.0f);
	}
	return glm::vec2(brain->step) / detail::k_MapCoordsPerMetre;
}

/// Living::GetFinalState-based Living::IsAvailableForReaction (0x5F11F0)
bool IsAvailableForReaction(const AnimalBrain& brain)
{
	if ((brain.status & 1) != 0)
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(brain.topState, 52));
	const auto final = static_cast<AnimalState>(info.field0xc != 0 ? brain.topState : brain.finalState);
	switch (final)
	{
	case AnimalState::Dead:
	case AnimalState::Drowning:
	case AnimalState::Downed:
	case AnimalState::BeingEaten:
	case AnimalState::Dying:
	case AnimalState::WaitForAnimation:
	case static_cast<AnimalState>(5): // IN_DANCE
		return false;
	default:
		return true;
	}
}

/// Living::FleeFromPredatorPriority (0x5F15D0; a prowling predator, at its speed2 or slower, goes unnoticed) and
/// Lion::FleeFromPredatorPriority (0x4203C0; a predator only flees from a stronger one)
uint32_t Priority(AnimalInfo type, const GAnimalInfo& info, entt::entity predator)
{
	const auto& predatorInfo = detail::InfoOf(Locator::entitiesRegistry::value().Get<const Animal>(predator));
	if (IsPredator(type))
	{
		return predatorInfo.strength > info.strength ? Reaction().priority : 0;
	}
	return SpeedOf(predator) <= static_cast<uint32_t>(predatorInfo.speedGroup.speed2) ? 0 : Reaction().priority;
}

/// StandardNumGameTurnsToReactFunction (0x5F18C0) through NumGameTurnsToReactToPredatorFunction (0x5F1980)
uint32_t TurnsToReact(const AnimalBrain& brain, float d, bool fastAndComing)
{
	if (brain.topState == static_cast<uint8_t>(AnimalState::LookingAtObjectReaction) && fastAndComing)
	{
		return 0;
	}
	const auto& reaction = Reaction();
	const float range = 10.0f + reaction.maxReactionDistance;
	return static_cast<uint32_t>(static_cast<float>(reaction.numGameTurnsForNormalThingsToReact) *
	                             (1.0f + 0.5f * reaction.howImportantIsDistance * (range - d) / range));
}

/// fn_005F1E60: the predator comes at it (its movement within acos 0.8 of the line to it)
bool ComingTowards(glm::vec2 me, glm::vec2 predator, glm::vec2 movement)
{
	if (movement == glm::vec2(0.0f) || me == predator)
	{
		return false;
	}
	return glm::dot(glm::normalize(me - predator), glm::normalize(movement)) >= 0.8f;
}

/// Animal::GetFleeingPositionFromMovingObject (0x420550): ~10 m sideways out of its path, on the side it stands on,
/// +-4 m; a still predator: straight away from it (GetFleeingPositionFromStationaryObject [inferred])
glm::vec2 FleeingPosition(glm::vec2 me, glm::vec2 predator, glm::vec2 movement, float distance)
{
	if (movement == glm::vec2(0.0f))
	{
		const glm::vec2 away = me - predator;
		return glm::length(away) > 0.0f ? me + glm::normalize(away) * distance : me + glm::vec2(distance, 0.0f);
	}
	auto& rng = Locator::rng::value();
	const glm::vec2 dir = glm::normalize(movement);
	const glm::vec2 perp(-dir.y, dir.x);
	const glm::vec2 f(rng.NextValue(0.0f, 8.0f) + perp.x * distance - 4.0f, rng.NextValue(0.0f, 8.0f) + perp.y * distance - 4.0f);
	return glm::dot(perp, me - predator) >= 0.0f ? me + f : me - f;
}

/// StopReactingAndSetState (0x5F11C0): Animal::ResetStateAfterReacting, back to the stored state's return state
/// (info.dat animalStateTable field0x1c: DECIDE_WHAT_TO_DO; 0 for the moving states, taken as DECIDE too)
void StopReacting(detail::Context& ctx)
{
	const auto& info = Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(ctx.brain.previousState, 52));
	const auto back = info.field0x1c != 0 ? static_cast<AnimalState>(info.field0x1c) : AnimalState::DecideWhatToDo;
	ctx.brain.reacting = false;
	ctx.brain.predator = entt::null;
	detail::SetTopState(ctx, back);
}

bool PredatorGone(const detail::Context& ctx)
{
	return ctx.brain.predator == entt::null || !detail::Available(ctx.brain.predator);
}

glm::vec2 PredatorPos(const detail::Context& ctx)
{
	return detail::Xz(Locator::entitiesRegistry::value().Get<const Transform>(ctx.brain.predator));
}
} // namespace

void SpreadPredatorReaction(entt::entity predator)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !registry.AllOf<Animal, Transform>(predator) ||
	    !IsPredator(registry.Get<const Animal>(predator).type))
	{
		return;
	}
	// SpreadReaction (0x6E3E10): max(1, trunc(radius * 0.2))^2 map cells around it whose centre is within the radius;
	// ApplyReactionToLivingObjectsAtSquare (0x6E3F90) for every animal in them
	const auto& reaction = Reaction();
	const glm::vec2 at = detail::Xz(registry.Get<const Transform>(predator));
	const auto centre = detail::CellOf(at);
	const int side = std::max(1, static_cast<int>(reaction.maxReactionDistance * 0.2f));
	std::vector<entt::entity> offered;
	registry.Each<const Animal, const Transform>([&](entt::entity entity, const Animal&, const Transform& transform) {
		if (entity == predator)
		{
			return;
		}
		const glm::vec2 p = detail::Xz(transform);
		const auto cell = detail::CellOf(p);
		const int dx = static_cast<int>(cell.x) - static_cast<int>(centre.x);
		const int dz = static_cast<int>(cell.y) - static_cast<int>(centre.y);
		// the spiral's square block of side x side cells [the odd-side block around its own cell]
		if (std::abs(dx) > side / 2 || std::abs(dz) > side / 2 ||
		    glm::distance(MapInterface::GetCellCenter(cell), at) > reaction.maxReactionDistance)
		{
			return;
		}
		offered.push_back(entity);
	});
	for (const auto entity : offered)
	{
		const auto& animal = registry.Get<const Animal>(entity);
		const auto& info = detail::InfoOf(animal);
		// info.isReacting[28] (isFleeingFromPredator) and within reach: d = half the Manhattan distance
		const glm::vec2 p = detail::Xz(registry.Get<const Transform>(entity));
		const float d = (std::abs(p.x - at.x) + std::abs(p.y - at.y)) * 0.5f;
		if (info.isReacting.isFleeingFromPredator == 0 || d > reaction.maxReactionDistance ||
		    Priority(animal.type, info, predator) == 0)
		{
			continue;
		}
		auto* brain = detail::BrainOf(entity);
		if (brain == nullptr || brain->reacting || !IsAvailableForReaction(*brain))
		{
			continue;
		}
		// Animal::SetupFleeFromPredator (0x420410) -> Living::AddReaction (0x5F0F30): StorePreviousState, state 49
		brain->previousState = brain->topState;
		brain->reacting = true;
		brain->predator = predator;
		brain->reactStart = detail::g_Turn;
		detail::SetTopState(entity, *brain, AnimalState::FleeingFromPredatorReaction);
	}
}

namespace detail
{

void ProcessReaction(Context& ctx)
{
	if (!ctx.brain.reacting)
	{
		return;
	}
	if (PredatorGone(ctx))
	{
		StopReacting(ctx);
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PredatorPos(ctx);
	const float d = glm::distance(me, at);
	const bool fastAndComing = Metres(SpeedOf(ctx.brain.predator)) * 10.0f > 5.0f && ComingTowards(me, at, MovementOf(ctx.brain.predator));
	if (g_Turn - ctx.brain.reactStart > TurnsToReact(ctx.brain, d, fastAndComing))
	{
		StopReacting(ctx);
	}
}

void FleeingFromPredatorReaction(Context& ctx)
{
	// Animal::FleeingFromPredatorReaction (0x4201F0)
	if (ctx.brain.predator == entt::null)
	{
		return;
	}
	if (PredatorGone(ctx))
	{
		StopReacting(ctx);
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PredatorPos(ctx);
	if (glm::distance(me, at) > Reaction().maxDistanceToRunAwayFromObject)
	{
		StopReacting(ctx);
		return;
	}
	const auto p = FleeingPosition(me, at, MovementOf(ctx.brain.predator), 10.0f);
	if (InBounds(p))
	{
		SetSpeed(ctx, static_cast<uint32_t>(ctx.info.speedGroup.speedFleeing));
		SetupMoveToPos(ctx, p, AnimalState::FleeingFromObjectReaction);
	}
}

void FleeingFromObjectReaction(Context& ctx)
{
	// Living::FleeingFromObjectReaction (0x5F1D10) -> FleeFromObjectIfComingTowardsMe(pred, 30, 30) (0x5F1D90)
	if (!ctx.brain.reacting || PredatorGone(ctx))
	{
		StopReacting(ctx);
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PredatorPos(ctx);
	const float d = glm::distance(me, at);
	if (d > Reaction().maxDistanceToRunAwayFromObject)
	{
		StopReacting(ctx);
		return;
	}
	const auto movement = MovementOf(ctx.brain.predator);
	if (d > 30.0f && !ComingTowards(me, at, movement))
	{
		SetTopState(ctx, AnimalState::FleeingAndLookingAtObjectReaction);
		ctx.brain.angle = AngleOf(at - me);
		FaceAngle(ctx.transform, ctx.brain.angle);
		return;
	}
	const auto p = FleeingPosition(me, at, movement, 10.0f);
	if (InBounds(p))
	{
		SetupMoveToPos(ctx, p, AnimalState::FleeingFromObjectReaction);
	}
}

void FleeingAndLookingReaction(Context& ctx)
{
	// Living::LookingAtObjectReaction (0x5F23A0): watches it, until it is 75 m away
	if (!ctx.brain.reacting || PredatorGone(ctx))
	{
		StopReacting(ctx);
		return;
	}
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 at = PredatorPos(ctx);
	if (glm::distance(me, at) > Reaction().maxDistanceToRunAwayFromObject)
	{
		StopReacting(ctx);
		return;
	}
	// LookAtObject: turns to face it
	SetTowardsAngle(ctx, AngleOf(at - me), glm::distance(me, at));
	FaceAngle(ctx.transform, ctx.brain.angle);
}

} // namespace detail
} // namespace openblack::ecs::animal_ai
