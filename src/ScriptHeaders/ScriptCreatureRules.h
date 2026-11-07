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

/// How the scripts' creature functions read and set a creature, where the answer is a rule of its own
namespace openblack::script::creature_rules
{

/// The area a script keeps a creature in. The centre is kept as precisely as a map position, at height 0; the height
/// the script gives is not used.
struct Confinement
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};
[[nodiscard]] Confinement ConfinementFor(const glm::vec3& point, float radius);

} // namespace openblack::script::creature_rules
