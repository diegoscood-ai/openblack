/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A sprite drawn over a disciple: its glow, or the symbol of its work shown in or under the hand (ecs::disciple_marks)
struct DiscipleMark
{
	entt::entity villager {entt::null};
	bool symbol {false};
	/// Where it is drawn, over its villager's drawing pose (ECS/MobileDrawing.h, PoseOf::Drawing); its Transform keeps
	/// the game's
	std::optional<glm::vec3> drawPosition;
};

} // namespace openblack::ecs::components
