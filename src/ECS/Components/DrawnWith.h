/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A model drawn inside another object's draw, as a village totem's icon and its second, see-through totem are drawn
/// in the totem's: it takes the owner's place in the frame's object draw, at a step of that draw (the owner's own model
/// is graphics::object_order::k_OwnStep; lower ones are drawn before it, higher ones after)
struct DrawnWith
{
	entt::entity owner {entt::null};
	uint8_t step {2};
};

} // namespace openblack::ecs::components
