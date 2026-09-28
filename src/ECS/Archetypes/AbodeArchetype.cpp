/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeArchetype.h"

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PotArchetype.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void AddStoragePitComponents(entt::entity entity, const Mesh& pitMesh, const GAbodeInfo& info, const glm::vec3& position,
                             float yAngleRadians, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& potInfoConstants = Locator::infoConstants::value().pot;

	const auto l3dMesh = entt::locator<resources::ResourcesInterface>::value().GetMeshes().Handle(pitMesh.id);
	const auto& extraMetrics = l3dMesh->GetExtraMetrics();

	auto& pit = registry.Assign<StoragePit>(entity);

	size_t i = 0;
	for (auto type = info.potForResourceWood; type != PotInfo::_COUNT;
	     type = potInfoConstants.at(static_cast<size_t>(type)).nextPotForResource)
	{
		const auto& m = extraMetrics.at(i);
		auto translation = static_cast<glm::vec3>(glm::eulerAngleY(-yAngleRadians) * m[3]);
		pit.woodPiles.at(i) = PotArchetype::Create(position + translation, yAngleRadians, type, 0, true);
		++i;
	}
	assert(i == pit.woodPiles.size());
	const auto& m = extraMetrics.at(5);
	auto translation = static_cast<glm::vec3>(glm::eulerAngleY(-yAngleRadians) * m[3]);
	pit.foodPile = PotArchetype::Create(position + translation, yAngleRadians, info.potForResourceFood, 0, true);
	// The store's totals are spread over its piles as StoragePit::AddResource does.
	AbodeArchetype::AddToStoragePit(entity, ResourceType::Wood, woodAmount);
	AbodeArchetype::AddToStoragePit(entity, ResourceType::Food, foodAmount);
}

entt::entity AbodeArchetype::Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
                                    float scale, uint32_t foodAmount, uint32_t woodAmount)
{
	auto& registry = Locator::entitiesRegistry::value();

	// If there is no town, assign to closest
	if (registry.Context().towns.find(townId) == registry.Context().towns.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Function {} has invalid Town ({}).", __func__, townId);
		const auto town = Locator::townSystem::value().FindClosestTown(position);
		if (town != entt::null)
		{
			townId = registry.Get<Town>(town).id;
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Function {} has could not find closest town.", __func__, townId);
			return entt::null;
		}
	}

	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(type));
	bool morphsWithTerrain = false;
	morphsWithTerrain |= info.abodeType == AbodeType::Graveyard;
	morphsWithTerrain |= info.abodeType == AbodeType::StoragePit;
	if (info.abodeType == AbodeType::Wonder)
	{
		morphsWithTerrain |= info.tribeType == Tribe::CELTIC;
		morphsWithTerrain |= info.tribeType == Tribe::JAPANESE;
		morphsWithTerrain |= info.tribeType == Tribe::INDIAN;
		morphsWithTerrain |= info.tribeType == Tribe::NORSE;
		morphsWithTerrain |= info.tribeType == Tribe::TIBETAN;
	}
	morphsWithTerrain |= info.abodeType == AbodeType::Workshop;
	morphsWithTerrain |= info.abodeType == AbodeType::Citadel;
	morphsWithTerrain |= info.abodeType == AbodeType::Creche;
	morphsWithTerrain |= info.abodeType == AbodeType::FootballPitch;
	morphsWithTerrain |= info.abodeType == AbodeType::TownCentre;
	morphsWithTerrain |= info.abodeType == AbodeType::Field;

	const auto& transform =
	    registry.Assign<Transform>(entity, position, glm::mat3(glm::eulerAngleY(-yAngleRadians)), glm::vec3(scale));
	registry.Assign<Abode>(entity, info.abodeNumber, townId, foodAmount, woodAmount);
	auto resourceId = resources::HashIdentifier(info.meshId);
	const auto& mesh = registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (morphsWithTerrain)
	{
		registry.Assign<MorphWithTerrain>(entity);
	}

	// Create Fixed component with a 2d bounding circle
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);

	switch (info.abodeType)
	{
	case AbodeType::StoragePit:
		AddStoragePitComponents(entity, mesh, info, position, yAngleRadians, foodAmount, woodAmount);
		break;
	default:
		break;
	}

	return entity;
}

namespace
{
void SyncStoreTotals(entt::entity store)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* abode = registry.TryGet<Abode>(store);
	if (abode == nullptr)
	{
		return;
	}
	abode->woodAmount = AbodeArchetype::StoragePitAmount(store, ResourceType::Wood);
	abode->foodAmount = AbodeArchetype::StoragePitAmount(store, ResourceType::Food);
}

Pot* StorePot(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	return pile != entt::null && registry.Valid(pile) ? registry.TryGet<Pot>(pile) : nullptr;
}
} // namespace

uint32_t AbodeArchetype::AddToStoragePit(entt::entity store, ResourceType type, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr || amount == 0)
	{
		return 0;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	uint32_t added = 0;
	const auto fill = [&](entt::entity pile) {
		auto* pot = StorePot(pile);
		if (pot == nullptr || amount == 0)
		{
			return;
		}
		// Pot::JustAddResource 0x66D2B0 clips at maxAmountInPot only when nextPotForResource is set.
		const auto& info = pots.at(static_cast<size_t>(pot->type));
		const uint32_t cap = info.nextPotForResource != PotInfo::_COUNT ? info.maxAmountInPot : 65535u;
		const uint32_t room = cap > pot->amount ? cap - pot->amount : 0u;
		const uint32_t n = std::min(amount, room);
		if (n == 0)
		{
			return;
		}
		pot->amount = static_cast<uint16_t>(pot->amount + n);
		amount -= n;
		added += n;
		PotArchetype::SetSize(pile, true);
	};
	if (type == ResourceType::Wood)
	{
		for (const auto pile : pit->woodPiles)
		{
			fill(pile);
		}
	}
	else
	{
		fill(pit->foodPile);
	}
	SyncStoreTotals(store);
	return added;
}

uint32_t AbodeArchetype::RemoveFromStoragePit(entt::entity store, ResourceType type, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr || amount == 0)
	{
		return 0;
	}
	uint32_t removed = 0;
	const auto take = [&](entt::entity pile) {
		auto* pot = StorePot(pile);
		if (pot == nullptr || amount == 0 || pot->amount == 0)
		{
			return;
		}
		const uint32_t n = std::min<uint32_t>(amount, pot->amount);
		pot->amount = static_cast<uint16_t>(pot->amount - n);
		amount -= n;
		removed += n;
		PotArchetype::SetSize(pile, true);
	};
	if (type == ResourceType::Wood)
	{
		for (auto it = pit->woodPiles.rbegin(); it != pit->woodPiles.rend(); ++it)
		{
			take(*it);
		}
	}
	else
	{
		take(pit->foodPile);
	}
	SyncStoreTotals(store);
	return removed;
}

uint32_t AbodeArchetype::StoragePitAmount(entt::entity store, ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr)
	{
		return 0;
	}
	uint32_t total = 0;
	if (type == ResourceType::Wood)
	{
		for (const auto pile : pit->woodPiles)
		{
			if (const auto* pot = StorePot(pile); pot != nullptr)
			{
				total += pot->amount;
			}
		}
	}
	else if (const auto* pot = StorePot(pit->foodPile); pot != nullptr)
	{
		total += pot->amount;
	}
	return total;
}

entt::entity AbodeArchetype::StoragePitOfPile(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity owner = entt::null;
	registry.Each<const StoragePit>([&](entt::entity store, const StoragePit& pit) {
		if (pit.foodPile == pile || std::find(pit.woodPiles.begin(), pit.woodPiles.end(), pile) != pit.woodPiles.end())
		{
			owner = store;
		}
	});
	return owner;
}
