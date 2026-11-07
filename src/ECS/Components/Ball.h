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

#include <optional>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The football: a thing lying about, of the ball's own table row, that the hand and creatures play with as a toy
struct Ball
{
	/// Where it was last kicked at
	std::optional<glm::vec3> destination;
	/// Its point in the world as its pitch's turn last took it, and the one that turn before: the goal test looks at
	/// the way between them. Both the point it is made at
	glm::vec3 point {0.0f};
	glm::vec3 previousPoint {0.0f};
	/// Set by a kick (the bits of its launch's x speed): a ball no kick has set moving never scores
	uint32_t kick {0};
};

} // namespace openblack::ecs::components
