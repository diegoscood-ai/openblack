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

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "Common/RandomNumberManager.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::components;

namespace
{
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

void openblack::ecs::UpdateFishShoals(float seconds, const glm::vec3& camera)
{
	// the frame's game time in seconds, at most 0.1
	const float dt = std::min(seconds, 0.1f);
	auto& rng = Locator::rng::value();
	Locator::entitiesRegistry::value().Each<FishFarm>([&](FishFarm& farm) {
		if (!farm.shoal.has_value())
		{
			return;
		}
		auto& shoal = *farm.shoal;
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
		for (auto& fish : shoal.fish)
		{
			UpdateFish(fish, shoal.target, dt);
		}
	});
}
