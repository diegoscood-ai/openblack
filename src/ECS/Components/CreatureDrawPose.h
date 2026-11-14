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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where a creature's body is drawn this frame: between where it was at the start of the turn and where it is at its
/// end. Its Transform moves only once a turn.
struct CreatureDrawPose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// The scale it is drawn at, as its body was last given its size (each turn, from its own size after its spells, or
	/// smaller in its temple's pen); its Transform keeps its own. None before its body is first given a size
	std::optional<glm::vec3> scale;
	/// The size it is drawn at then, kept between its limits, which its radius on the ground follows; its own is
	/// Creature::size
	std::optional<float> size;
	/// The size its body was given, as given: its walking speeds go by it
	std::optional<float> given;
	/// Whether that size is its temple's pen's
	bool inPen {false};
};

} // namespace openblack::ecs::components
