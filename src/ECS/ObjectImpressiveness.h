/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::object
{

/// How impressive an object's kind is: its info's impressive value by the land's balance of impressiveness. Every
/// class of object answers the same; anything without an object info is worth 0
[[nodiscard]] float ImpressiveValue(entt::entity object);

} // namespace openblack::ecs::object
