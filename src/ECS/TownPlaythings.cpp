/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownPlaythings.h"

#include <algorithm>
#include <limits>

#include "Common/GameDistance.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"

namespace openblack::ecs::town_playthings
{

bool Add(std::vector<entt::entity>& playthings, entt::entity thing,
         const std::function<bool(entt::entity)>& stillAboutOfSameKind)
{
	if (std::ranges::any_of(playthings, stillAboutOfSameKind))
	{
		return false;
	}
	playthings.insert(playthings.begin(), thing);
	return true;
}

std::optional<entt::entity> Nearest(std::span<const Candidate> candidates, glm::vec3 point)
{
	std::vector<Candidate> ordered(candidates.begin(), candidates.end());
	std::ranges::stable_sort(ordered, [](const Candidate& a, const Candidate& b) {
		return a.owner != b.owner ? static_cast<uint32_t>(a.owner) < static_cast<uint32_t>(b.owner) : a.id < b.id;
	});
	std::optional<entt::entity> nearest;
	float best = std::numeric_limits<float>::max();
	for (const auto& candidate : ordered)
	{
		const float distance = game_math::GetDistanceInMetres(point, candidate.position);
		if (distance < best)
		{
			best = distance;
			nearest = candidate.town;
		}
	}
	return nearest;
}

bool GiveBall(Registry& registry, entt::entity town, entt::entity ball)
{
	return Add(registry.Get<components::Town>(town).playthings, ball, [&registry](entt::entity thing) {
		return registry.Valid(thing) && !registry.AllOf<components::Unavailable>(thing) &&
		       registry.AllOf<components::Ball>(thing);
	});
}

bool IsBallFree(const Registry& registry, entt::entity ball)
{
	const auto* component = registry.Valid(ball) ? registry.TryGet<const components::Ball>(ball) : nullptr;
	return component != nullptr && !component->owned;
}

bool AddBallToTown(Registry& registry, entt::entity ball, entt::entity town)
{
	auto* component = registry.TryGet<components::Ball>(ball);
	if (component == nullptr)
	{
		return false;
	}
	if (component->town != entt::null)
	{
		return true;
	}
	if (!GiveBall(registry, town, ball))
	{
		return false;
	}
	component = &registry.Get<components::Ball>(ball);
	component->town = town;
	component->owned = true;
	return true;
}
} // namespace openblack::ecs::town_playthings
