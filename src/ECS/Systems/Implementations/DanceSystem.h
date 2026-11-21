/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include "ECS/Systems/DanceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class DanceSystem final: public DanceSystemInterface
{
public:
	entt::entity Create(DanceInfo type, const map_coords::MapCoords& place, entt::entity owner, uint32_t duration,
	                    bool madeByScript, entt::entity town) override;
	[[nodiscard]] std::shared_ptr<const dance::DanceFile> FileOf(DanceInfo type) override;
	void Destroy(entt::entity dance) override;
	void ProcessTurn() override;
	[[nodiscard]] const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes() override;
	uint32_t NextUntitledNumber() override { return _nextUntitled++; }

private:
	/// The shapes, read once they are first needed
	std::optional<std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>> _shapes;
	/// The game's count of dance groups made, starting at 1 (no game's close resets it in openblack: the program ends
	/// with the game)
	uint32_t _nextUntitled {1};
};

} // namespace openblack::ecs::systems
