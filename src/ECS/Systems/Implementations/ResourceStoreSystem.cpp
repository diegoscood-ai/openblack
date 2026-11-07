/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ResourceStoreSystem.h"

#include <entt/entity/entity.hpp>

#include "ECS/ObjectDelivery.h"
#include "ECS/ObjectResources.h"
#include "ECS/PotResource.h"
#include "ECS/ResourceStores.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

ResourceStoreSystemInterface::ObjectResource ResourceStoreSystem::ResourceOf(entt::entity object) const
{
	const auto [type, amount] = object_delivery::ResourceOf(object);
	return {.type = type, .amount = amount, .poisoned = object_resources::IsPoisoned(object)};
}

bool ResourceStoreSystem::IsStore(entt::entity store, ResourceType type) const
{
	return resource_stores::IsResourceStore(store, type);
}

uint32_t ResourceStoreSystem::AddToStore(entt::entity store, ResourceType type, uint32_t amount,
                                         std::optional<PlayerNames> giver, bool poisoned)
{
	return object_resources::AddResource(store, type, amount, pot_resource::DropperOf(giver), poisoned);
}

uint32_t ResourceStoreSystem::AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned)
{
	return pot_resource::AddToPotDirect(pile, type, amount, poisoned);
}

uint32_t ResourceStoreSystem::TakeFromPile(entt::entity pile, ResourceType type, uint32_t amount,
                                           std::optional<PlayerNames> taker)
{
	return object_resources::RemoveResource(pile, type, amount, pot_resource::DropperOf(taker));
}

bool ResourceStoreSystem::TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver)
{
	return resource_stores::DeleteObjectAndTakeResource(store, object, pot_resource::DropperOf(giver));
}

bool ResourceStoreSystem::PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player,
                                 bool poisoned)
{
	const auto dropper = pot_resource::DropperOf(magic::players::IsHuman(player) ? std::optional(player) : std::nullopt);
	// into the stores and piles about there, or onto a new pile
	entt::entity pile = entt::null;
	const auto stored = pot_resource::AddResourceToPos(point, dropper, type, amount, poisoned, speedUp, &pile);
	return stored > 0 || pile != entt::null;
}

std::optional<entt::entity> ResourceStoreSystem::StoreOf(entt::entity pile) const
{
	const auto structure = resource_stores::StructureOfPile(pile);
	return structure != entt::null ? std::optional(structure) : std::nullopt;
}
