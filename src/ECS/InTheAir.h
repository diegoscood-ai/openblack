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

namespace openblack::ecs
{

/// Whether a thing in the physics is still really in the air: in the physics (flying, not a resting body) with a body,
/// and fast or above the land, the land read at the map point its centre truncates to. The villagers watching a thing
/// fly and the creatures watching one both ask this.
[[nodiscard]] bool ActuallyInTheAir(entt::entity object);

} // namespace openblack::ecs
