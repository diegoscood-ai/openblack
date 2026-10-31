/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDeletion.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::town_deletion
{
using components::Town;

namespace
{
/// A homeless villager whose town is deleted: one of the vagrants, starting out as one, with no town
void LeaveAsVagrant(entt::entity villager)
{
	town_villagers::AddToVagrants(villager);
	villager::SetTopState(villager, VillagerStates::VagrantStart);
	villager::SetTown(villager, entt::null);
}
} // namespace

bool EmptiedForGood(size_t buildings, size_t plans, uint32_t people)
{
	return buildings == 0 && plans == 0 && people == 0;
}

void ToBeDeleted(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
	if (t == nullptr || t->deleting)
	{
		return;
	}
	t->deleting = true;
	const auto id = t->id;
	// It leaves its player, as a change of hands does
	town_belief::RemoveTownFromPlayer(town);
	// Its homeless, the head first, each out of the list; one still in the game goes off as a vagrant
	while (!town_villagers::Homeless(town).empty())
	{
		const auto villager = town_villagers::Homeless(town).front();
		static_cast<void>(town_villagers::RemoveFromHomelessList(town, villager));
		if (ecs::IsAvailable(villager))
		{
			LeaveAsVagrant(villager);
		}
	}
	// Each building still in it leaves it
	for (const auto abode : town_stats::AbodesOf(town))
	{
		abodes::RemoveStructureFromTown(town, abode);
	}
	// Its animals have no town any more
	t = &registry.Get<Town>(town);
	for (const auto animal : std::exchange(t->animals, {}))
	{
		if (registry.Valid(animal))
		{
			animal_ai::SetTown(animal, entt::null);
		}
	}
	// Its building sites are deleted
	for (const auto site : std::vector<entt::entity>(registry.Get<const Town>(town).buildingSites))
	{
		ecs::ToBeDeleted(site);
	}
	t = &registry.Get<Town>(town);
	t->flocks.clear();
	t->creatureRecords.clear();
	t->missionaries.clear();
	// Its need flags are deleted, each one's slot emptied
	if (auto* flags = registry.TryGet<components::TownDesireFlags>(town); flags != nullptr)
	{
		for (auto& flag : flags->flags)
		{
			if (flag != entt::null && registry.Valid(flag))
			{
				const auto deleted = std::exchange(flag, entt::null);
				ecs::ToBeDeleted(deleted);
			}
			else
			{
				flag = entt::null;
			}
		}
	}
	// The rewards given to it are deleted, in the order they were given
	std::vector<std::pair<uint32_t, entt::entity>> rewards;
	registry.Each<const components::Reward>([&rewards, town](entt::entity entity, const components::Reward& reward) {
		if (reward.town == town)
		{
			rewards.emplace_back(reward.serial, entity);
		}
	});
	std::ranges::sort(rewards);
	for (const auto& [serial, reward] : rewards)
	{
		ecs::ToBeDeleted(reward);
	}
	// Out of the game's list of towns, then the influence is drawn afresh
	registry.Context().towns.erase(id);
	influence::ForceNeedUpdateInfluence();
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Town {} deleted: no building, plan or villager left in it", id);
	}
}

} // namespace openblack::ecs::town_deletion
