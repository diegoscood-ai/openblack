/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TownSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class TownSystem final: public TownSystemInterface
{
public:
	/// The forwards to our town code (town_villagers, abode_villagers, villager, map_cells, town_queries,
	/// town_emergency, plans); the points in metres, where ours works in whole map units
	[[nodiscard]] entt::entity FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const override;
	/// The nearest town at any distance
	[[nodiscard]] entt::entity FindClosestTown(const glm::vec3& point) const override;
	bool AddVillagerToTown(entt::entity town, entt::entity villager) override;
	void AddVillagerToAbode(entt::entity abode, entt::entity villager) override;
	/// Changes no state of the villager's: villager::MakeHomeless is this and the homeless state
	bool MakeHomeless(entt::entity villager) override;
	[[nodiscard]] bool CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter,
	                                     entt::entity ignore) const override;
	[[nodiscard]] std::optional<glm::vec2> FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
	                                                     ClearAreaFilter filter, entt::entity ignore) const override;
	glm::vec2 GetCongregationPos(entt::entity town) override;
	void BuildingCreated(entt::entity town, glm::vec2 position, float radius) override;
	[[nodiscard]] bool IsInStateOfEmergency(entt::entity town) const override;
	void SetInStateOfEmergency(entt::entity town) override;
	/// Not called by the game: each town's own turn runs its emergency (town_process::ProcessTown, step 15)
	void ProcessTurn() override;
};
} // namespace openblack::ecs::systems
