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

namespace openblack
{
struct GLivingInfo;
}

namespace openblack::ecs::living
{

/// The living part of every villager, animal and creature, as it is made: it draws a number below half its info's
/// start age (none when that half is 0) and takes that plus three quarters of the start age, rounded as whole numbers,
/// as its age. The kind made sets its own age afterwards, so only the draw is left for most. Returns that age
[[nodiscard]] uint32_t ConstructorAgeDraw(const GLivingInfo& info);

} // namespace openblack::ecs::living
