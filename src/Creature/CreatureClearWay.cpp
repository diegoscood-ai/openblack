/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureClearWay.h"

#include <algorithm>

#include "Creature/CreatureDesires.h"

using namespace openblack;

std::optional<uint32_t> creature_clear_way::ClearingAction(const Obstacle& obstacle, std::span<const Miracle> miracles,
                                                           const Actions& actions)
{
	if (obstacle.templeHeart || obstacle.storagePit || obstacle.townCentre)
	{
		return std::nullopt;
	}
	if (obstacle.canPickUp)
	{
		return actions.throwAround;
	}
	if (obstacle.canKick)
	{
		return actions.kick;
	}
	constexpr auto k_Anger = static_cast<uint32_t>(creature_desires::Desire::Anger);
	const auto miracle =
	    std::ranges::find_if(miracles, [](const Miracle& m) { return m.known && m.action != 0 && m.desire == k_Anger; });
	if (miracle == miracles.end())
	{
		return std::nullopt;
	}
	return miracle->action;
}

bool creature_clear_way::MayNote(std::span<const entt::entity> met, entt::entity thing)
{
	return met.size() < k_MostMet && std::ranges::find(met, thing) == met.end();
}

std::optional<size_t> creature_clear_way::Choose(size_t count, const std::function<uint32_t(uint32_t)>& random)
{
	if (count == 0)
	{
		return std::nullopt;
	}
	const auto all = static_cast<uint32_t>(count);
	const auto from = random(k_AnyOneIn) == 0 ? all : std::min(all, k_FirstFew);
	return static_cast<size_t>(random(from));
}

float creature_clear_way::WalkOnDistance(float height)
{
	// The height is kept unless it is below the least distance (a height that is not a number is kept too)
	return height < k_LeastWalkOnDistance ? k_LeastWalkOnDistance : height;
}

std::vector<creature_mind::Step> creature_clear_way::WalkOn(glm::vec2 destination, float height)
{
	using creature_mind::Gaze;
	using creature_mind::Movement;
	using creature_mind::Step;
	return {Step {
	    .kind = Step::Kind::Move,
	    .movement = {.kind = Movement::Kind::ToPoint, .point = destination, .maxDistance = WalkOnDistance(height)},
	    .gaze = Gaze {.point = glm::vec3 {destination.x, 0.0f, destination.y}, .goingTowards = true},
	}};
}
