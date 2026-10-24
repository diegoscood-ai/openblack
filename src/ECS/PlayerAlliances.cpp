/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerAlliances.h"

#include <cstdio>
#include <cstdlib>

#include <utility>

#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using components::PlayerAlliances;

namespace
{
/// The first entity of the player's name on this land, or entt::null; found through the const registry, which makes
/// no storage
entt::entity EntityOf(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	entt::entity found = entt::null;
	registry.Each<const components::Player>([&found, player](entt::entity entity, const components::Player& component) {
		if (found == entt::null && component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

/// Sets the player's strength towards a slot where their alliances are kept: on their entity, which gets them the
/// first time one is set (with what was kept for them until it was made), else in the player system
void SetStrength(PlayerNames player, size_t slot, float strength)
{
	if (!Locator::playerSystem::has_value())
	{
		std::fputs("ecs::alliances: no player system in the locator (Locator::playerSystem)\n", stderr);
		std::abort();
	}
	auto& players = Locator::playerSystem::value();
	const auto entity = EntityOf(player);
	if (entity == entt::null)
	{
		auto kept = players.AlliancesWithoutEntity(player).value_or(PlayerAlliances {});
		kept.strength.at(slot) = strength;
		players.KeepAlliancesWithoutEntity(player, kept);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* carried = registry.TryGet<PlayerAlliances>(entity); carried != nullptr)
	{
		carried->strength.at(slot) = strength;
		return;
	}
	auto alliances = players.AlliancesWithoutEntity(player).value_or(PlayerAlliances {});
	alliances.strength.at(slot) = strength;
	registry.AssignState<PlayerAlliances>(entity, alliances);
	players.KeepAlliancesWithoutEntity(player, std::nullopt);
}
} // namespace

float alliances::AlliesInfluence(const PlayerAlliances& of, const std::function<float(PlayerNames)>& rawOf)
{
	for (size_t slot = 0; slot < of.strength.size(); ++slot)
	{
		const auto ally = static_cast<PlayerNames>(slot);
		if (!MayUseInfluenceOf(of, ally))
		{
			continue;
		}
		if (const float raw = rawOf(ally); raw > 0.0f)
		{
			return raw;
		}
	}
	return 0.0f;
}

PlayerAlliances alliances::AlliancesOf(PlayerNames player)
{
	if (const auto entity = EntityOf(player); entity != entt::null)
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		if (const auto* carried = registry.TryGet<const PlayerAlliances>(entity); carried != nullptr)
		{
			return *carried;
		}
	}
	if (!Locator::playerSystem::has_value())
	{
		return {};
	}
	return Locator::playerSystem::value().AlliancesWithoutEntity(player).value_or(PlayerAlliances {});
}

float alliances::Strength(PlayerNames player, PlayerNames other)
{
	return Strength(AlliancesOf(player), other);
}

bool alliances::IsAllied(PlayerNames player, PlayerNames other)
{
	return IsAllied(AlliancesOf(player), other);
}

void alliances::SetAlliance(PlayerNames player, PlayerNames other, float strength)
{
	const auto otherIndex = static_cast<size_t>(other);
	const auto playerIndex = static_cast<size_t>(player);
	if (otherIndex >= PlayerAlliances::k_Players || playerIndex >= PlayerAlliances::k_Players)
	{
		return;
	}
	SetStrength(player, otherIndex, strength);
	SetStrength(other, playerIndex, strength);
}
