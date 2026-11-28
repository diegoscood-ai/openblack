/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/Components/CreatureFartSmoke.h"

#include <spdlog/spdlog.h>

#include "3D/ChimneySmoke.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureFart.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "Locator.h"

void openblack::creature_fart::StartSmoke(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	if (transform == nullptr)
	{
		return;
	}
	// at the anus as the creature is posed now, where it stands when it has no pose yet
	const auto at = ecs::creature_pose::GroinOf(creature).value_or(transform->position);
	registry.AssignOrReplace<ecs::components::ChimneySmoke>(
	    creature,
	    chimney_smoke::Create(at, chimney_smoke::k_HomeSmoke, [](float a, float b) { return game_random::crt::Random(a, b); }));
	const auto* mind = registry.TryGet<const ecs::components::CreatureMindState>(creature);
	registry.AssignOrReplace<ecs::components::CreatureFartSmoke>(
	    creature, ecs::components::CreatureFartSmoke {.planSerial = mind != nullptr ? mind->planSerial : 0u});
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		log->debug("Creature {} farts: its smoke starts at ({:.2f}, {:.2f}, {:.2f}) for plan {}", entt::to_integral(creature),
		           at.x, at.y, at.z, mind != nullptr ? mind->planSerial : 0u);
	}
	// the readout: where the fart's grey puffs start, their largest half size and how fast they fly behind the body
	const auto size = ecs::creature_pose::DrawnSize(registry, creature);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Creature {} farts at ({:.1f}, {:.1f}, {:.1f}): drawn size {:.2f}, puffs of half size {:.2f} blown "
	                   "behind at {:.2f} a second, its smoke at ({:.1f}, {:.1f}, {:.1f})",
	                   entt::to_integral(creature), transform->position.x, transform->position.y, transform->position.z, size,
	                   PuffSize(size), size * k_CloudPerSize, at.x, at.y, at.z);
}
