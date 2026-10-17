/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include "ECS/Systems/ResourceStoreSystemInterface.h"

namespace openblack::ecs::systems
{

/// The forwards to our store code (object_delivery, resource_stores, object_resources, pot_resource). A giver or a
/// taker is the local interface (pot_resource::DropperOf); none is no interface
class ResourceStoreSystem final: public ResourceStoreSystemInterface
{
public:
	[[nodiscard]] ObjectResource ResourceOf(entt::entity object) const override;
	[[nodiscard]] bool IsStore(entt::entity store, ResourceType type) const override;
	uint32_t AddToStore(entt::entity store, ResourceType type, uint32_t amount, std::optional<PlayerNames> giver,
	                    bool poisoned) override;
	uint32_t AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned) override;
	uint32_t TakeFromPile(entt::entity pile, ResourceType type, uint32_t amount, std::optional<PlayerNames> taker) override;
	bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver) override;
	/// Only a human player pours through its interface; any other player's pour has none
	bool PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player, bool poisoned) override;
	[[nodiscard]] std::optional<entt::entity> StoreOf(entt::entity pile) const override;
};

} // namespace openblack::ecs::systems
