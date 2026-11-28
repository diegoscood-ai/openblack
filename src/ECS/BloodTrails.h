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

#include <vector>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureMarks.h"

namespace openblack::ecs
{
class Registry;
}

/// The trails of blood a creature's bleeding wounds run down its skin (creature_marks::BloodTrail), stepped on its body
/// as the game's cut test poses it. Wiki: docs/wiki/creature/body.md#meshes-morph-layers-and-skin.
namespace openblack::ecs::blood_trails
{

/// A turn of the creature's own milliseconds on its trails: each running one is stepped down the skin, and the drops
/// of blood they leave, fresh, in the trails' order
[[nodiscard]] std::vector<creature_marks::Mark> Step(const Registry& registry, entt::entity creature,
                                                     creature_marks::BloodTrails& trails, int32_t ms);

} // namespace openblack::ecs::blood_trails
