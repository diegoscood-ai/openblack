/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LastDropped.h"

#include <utility>

#include <entt/entity/entity.hpp>

#include "ECS/Components/LastDropped.h"
#include "ECS/Components/Player.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The player's entity: the first one named so
std::optional<entt::entity> PlayerEntity(const Registry& registry, PlayerNames player)
{
	std::optional<entt::entity> found;
	registry.Each<const Player>([&found, player](entt::entity entity, const Player& component) {
		if (!found.has_value() && component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

void Keep(Registry& registry, PlayerNames player, entt::entity object)
{
	if (const auto entity = PlayerEntity(std::as_const(registry), player); entity.has_value())
	{
		registry.AssignOrReplaceState<LastDropped>(*entity, LastDropped {.object = object});
	}
}
} // namespace

void last_dropped::MarkThrown(Registry& registry, PlayerNames player, entt::entity object)
{
	Keep(registry, player, object);
}

void last_dropped::RecordDrop(Registry& registry, PlayerNames player, entt::entity object)
{
	// only the first interface found forgets it: another one that kept it too goes on keeping it
	if (const auto before = WhoLastDropped(std::as_const(registry), object); before.has_value())
	{
		Keep(registry, *before, entt::null);
	}
	Keep(registry, player, object);
}

std::optional<PlayerNames> last_dropped::WhoLastDropped(const Registry& registry, entt::entity object)
{
	if (object == entt::null)
	{
		return std::nullopt;
	}
	std::optional<PlayerNames> first;
	registry.Each<const Player>([&registry, &first, object](entt::entity entity, const Player& player) {
		if (player.name == PlayerNames::NEUTRAL || (first.has_value() && *first < player.name))
		{
			return;
		}
		// only the player's own entity counts (the first one named so), as for keeping the mark
		const auto* kept = registry.TryGet<const LastDropped>(entity);
		if (kept != nullptr && kept->object == object && PlayerEntity(registry, player.name) == entity)
		{
			first = player.name;
		}
	});
	return first;
}
