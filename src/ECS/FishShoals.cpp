/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishShoals.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "Common/RandomNumberManager.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::components;

namespace
{
/// [0xEB99F0] / [0xEA9F40]: set by the splashes of this frame, cleared after the shoals have seen it (fn_00824B90)
std::optional<glm::vec3> s_splash;

/// fn_008248E0
void UpdateFish(Fish& fish, const glm::vec3& target, float dt)
{
	fish.frame += dt * fish.speed * 25.0f;
	while (fish.frame >= 15.0f)
	{
		fish.frame -= 15.0f;
	}
	// a fish scared by a splash swims 4 times faster, easing back to normal over its last second
	float boost = 1.0f;
	if (fish.fleeTime > 0.0f)
	{
		boost = fish.fleeTime >= 1.0f ? 4.0f : 1.0f + 3.0f * fish.fleeTime;
		fish.fleeTime = std::max(0.0f, fish.fleeTime - dt);
	}
	fish.position += glm::vec3(std::cos(fish.heading), 0.0f, std::sin(fish.heading)) * fish.speed * dt * boost;
	// turn towards the shoal's target
	const float dx = target.x - fish.position.x;
	const float dz = target.z - fish.position.z;
	const float side = std::sin(fish.heading) * dx - std::cos(fish.heading) * dz < 0.0f ? 1.0f : -1.0f;
	fish.heading += side * fish.turnRate * dt;
	if (fish.heading > glm::pi<float>())
	{
		fish.heading -= glm::two_pi<float>();
	}
	else if (fish.heading <= -glm::pi<float>())
	{
		fish.heading += glm::two_pi<float>();
	}
}
} // namespace

void openblack::ecs::SplashWater(const glm::vec3& point)
{
	s_splash = point;
}

void openblack::ecs::ProcessFishFarmsTurn(uint32_t turn)
{
	if (turn % FishFarm::k_GrowthTurns != 0)
	{
		return;
	}
	Locator::entitiesRegistry::value().Each<FishFarm>(
	    [](FishFarm& farm) { farm.food = std::clamp(farm.food + 1.0f, 0.0f, FishFarm::k_FoodValue); });
}

std::optional<entt::entity> openblack::ecs::FindFishFarmAt(const glm::vec3& point)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
		if (found || !farm.shoal.has_value())
		{
			return;
		}
		const size_t count = std::min(farm.VisibleFish(), farm.shoal->fish.size());
		for (size_t i = 0; i < count; ++i)
		{
			const auto& fish = farm.shoal->fish[i].position;
			const float dx = fish.x - point.x;
			const float dz = fish.z - point.z;
			if (dx * dx + dz * dz < 4.0f)
			{
				found = entity;
				return;
			}
		}
	});
	return found;
}

uint32_t openblack::ecs::RemoveFishFarmFood(entt::entity farm, uint32_t amount)
{
	auto* fishFarm = Locator::entitiesRegistry::value().TryGet<FishFarm>(farm);
	if (fishFarm == nullptr)
	{
		return 0;
	}
	if (static_cast<float>(amount) <= fishFarm->food)
	{
		fishFarm->food -= static_cast<float>(amount);
		return amount;
	}
	const auto left = static_cast<uint32_t>(fishFarm->food);
	fishFarm->food = 0.0f;
	return left;
}

void openblack::ecs::UpdateFishShoals(float seconds, const glm::vec3& camera)
{
	// the frame's game time in seconds, at most 0.1
	const float dt = std::min(seconds, 0.1f);
	auto& rng = Locator::rng::value();
	const auto splash = s_splash;
	s_splash.reset();
	Locator::entitiesRegistry::value().Each<FishFarm>([&](FishFarm& farm) {
		if (!farm.shoal.has_value())
		{
			return;
		}
		auto& shoal = *farm.shoal;
		const size_t shown = std::min(farm.VisibleFish(), shoal.fish.size());
		shoal.shown = shown;
		// fn_00824DA0: nothing beyond 300 units; fading between 200 and ~224 units, then the original's alpha wraps
		// around as a byte (it goes negative), which leaves the far shoals nearly transparent
		const float d2 = glm::dot(shoal.centre - camera, shoal.centre - camera);
		shoal.visible = d2 <= 90000.0f;
		if (!shoal.visible)
		{
			return;
		}
		shoal.alpha = d2 > 40000.0f ? static_cast<uint8_t>(static_cast<int>((1.0f - (d2 - 40000.0f) * 1e-4f) * 255.0f) & 0xFF)
		                            : uint8_t {255};
		size_t fled = 0;
		if (splash.has_value())
		{
			// the shoal darts 2 units in a random direction; the fish within 8 units of the splash flee from it for 2 s
			const float r = rng.NextValue(0.0f, glm::two_pi<float>());
			shoal.target = shoal.centre + 2.0f * glm::vec3(std::cos(r), 0.0f, -std::sin(r));
			shoal.timer = 2.0f;
			for (size_t i = 0; i < shown; ++i)
			{
				auto& fish = shoal.fish[i];
				const auto away = fish.position - *splash;
				if (glm::dot(away, away) < 64.0f)
				{
					fish.fleeTime = 2.0f;
					fish.heading = std::atan2(away.z, away.x);
					++fled;
				}
			}
		}
		if (fled > 0 && std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish trace: splash, {} fish flee", fled);
		}
		if (dt <= 0.0f)
		{
			return;
		}
		shoal.timer -= dt;
		if (shoal.timer < 0.0f)
		{
			const auto previous = shoal.target;
			shoal.target = shoal.centre + glm::vec3(rng.NextValue(-FishShoal::k_Range, FishShoal::k_Range), 0.0f,
			                                        rng.NextValue(-FishShoal::k_Range, FishShoal::k_Range));
			shoal.timer = 0.5f * glm::distance(shoal.target, previous);
		}
		// the hidden fish (the stock is low) stay where they are until they come back
		for (size_t i = 0; i < shown; ++i)
		{
			UpdateFish(shoal.fish[i], shoal.target, dt);
		}
	});
}
