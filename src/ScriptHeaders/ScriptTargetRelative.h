/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// The scripts' "get target from A to B distance D angle T" (GET_TARGET_RELATIVE_POS)
/// (docs/wiki/scripts/script-commands.md#get_target_relative_pos-136)
namespace openblack::script::target_relative
{

/// The point distance away from to, along the direction from from to to turned by angle degrees about the vertical
/// axis: x becomes cos x - sin z and z becomes sin x + cos z, so a positive angle turns +z towards -x. The direction is
/// made one long first, and its height is kept as it is. When from and to are the same point there is no direction and
/// the answer is to itself. Every step rounds as the original's FPU does, so the bits match
[[nodiscard]] glm::vec3 Position(const glm::vec3& from, const glm::vec3& to, float distance, float angle);

} // namespace openblack::script::target_relative
