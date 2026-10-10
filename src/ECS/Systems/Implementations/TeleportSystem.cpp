/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TeleportSystem.h"

#include <cstddef>

#include <vector>

#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"

using namespace openblack;
using namespace openblack::ecs::systems;

bool TeleportSystem::CanPlaceStone(glm::vec3 point) const
{
	// no building, field, feature, other stone or the like within the stone's radius
	return !magic::teleport::AnyMultiCellStaticNear(point, magic::teleport::k_Radius);
}

bool TeleportSystem::ShouldReact(entt::entity stone, entt::entity living) const
{
	return magic::teleport::ShouldLivingThingReact(stone, living);
}

bool TeleportSystem::DoTeleport(entt::entity stone, entt::entity living, bool forced)
{
	return magic::teleport::DoTeleport(stone, living, forced) == 1;
}

bool TeleportSystem::DropOnStone(entt::entity villager, entt::entity stone, [[maybe_unused]] PlayerNames dropper)
{
	// whose stone it is was asked before the drop (CanDropOnStone): the villager's player, not the dropper
	return magic::teleport::ApplyVillagerDirectly(stone, villager) == 1;
}

std::optional<entt::entity> TeleportSystem::RouteStoneFor(PlayerNames player, glm::vec3 worshipper, glm::vec3 site,
                                                          float maxDistance) const
{
	// the player's stone list, newest first
	const auto& stones = magic::teleport::StonesOf(player);
	std::vector<glm::vec3> positions;
	positions.reserve(stones.size());
	for (const auto stone : stones)
	{
		positions.push_back(magic::teleport::MapPositionOf(stone));
	}
	const int index = magic::teleport::FindRouteStone(positions, magic::ToMap(worshipper), magic::ToMap(site), maxDistance);
	if (index < 0)
	{
		return std::nullopt;
	}
	return stones[static_cast<size_t>(index)];
}

void TeleportSystem::ProcessTurn()
{
	magic::teleport::ProcessPlayers();
}

void TeleportSystem::Reset()
{
	magic::teleport::Clear();
}

bool TeleportSystem::CanDropOnStone(entt::entity villager, entt::entity stone) const
{
	return magic::teleport::ValidToApplyVillagerDirectly(stone, villager);
}

void TeleportSystem::RegisterDestination(entt::entity stone, entt::entity living, glm::vec3 destination)
{
	magic::teleport::RegisterDestination(stone, living, destination);
}

void TeleportSystem::Update(float seconds)
{
	magic::teleport::UpdateFrame(seconds);
}

void TeleportSystem::RunDebugHooks()
{
	magic::teleport::RunDebugHooks();
}
