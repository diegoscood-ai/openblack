/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureMimicType.h"

namespace openblack::ecs
{
class Registry;

/// The kind of thing a creature copying the player tells a thing apart by (creature_mimic_type::Of), from what the
/// thing is: by the components that make it the game's kind of object
[[nodiscard]] creature_mimic_type::MimicType MimicTypeOf(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs
