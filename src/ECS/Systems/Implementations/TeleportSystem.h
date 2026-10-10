/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TeleportSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The game's teleport stones behind the service: each method hands on to the stones under Magic/Objects, which keep
/// the players' stone lists and the travellers
class TeleportSystem final: public TeleportSystemInterface
{
public:
	[[nodiscard]] bool CanPlaceStone(glm::vec3 point) const override;

	[[nodiscard]] bool ShouldReact(entt::entity stone, entt::entity living) const override;
	bool DoTeleport(entt::entity stone, entt::entity living, bool forced) override;
	bool DropOnStone(entt::entity villager, entt::entity stone, PlayerNames dropper) override;
	[[nodiscard]] std::optional<entt::entity> RouteStoneFor(PlayerNames player, glm::vec3 worshipper, glm::vec3 site,
	                                                        float maxDistance) const override;

	void ProcessTurn() override;
	void Reset() override;

	[[nodiscard]] bool CanDropOnStone(entt::entity villager, entt::entity stone) const override;
	void RegisterDestination(entt::entity stone, entt::entity living, glm::vec3 destination) override;
	void Update(float seconds) override;
	void RunDebugHooks() override;
};

} // namespace openblack::ecs::systems
