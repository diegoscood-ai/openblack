/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <vector>

#include "Map.h"

namespace openblack::ecs
{

/// The map's cells as the ordered lists of ecs::map_cells (MapCells.h), which its hooks keep as things are made, move
/// and go; Sync takes in what the other owners did without telling them
class MapProduction final: public MapInterface
{
public:
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& pos) const override;
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& pos) const override;
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 cell) const override;

	void Sync() override;
	void Refile(entt::entity entity) override;

private:
	/// The lists are linked through the things in them, so a view of a cell is copied out here
	mutable std::vector<entt::entity> _fixedView;
	mutable std::vector<entt::entity> _mobileView;
};

} // namespace openblack::ecs
