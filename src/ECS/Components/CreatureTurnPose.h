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
#include <vector>

#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// A creature's body as it is posed for the game turn: every bone in the world, by its index in the body's skeleton.
/// The game's logic reads its bones from here rather than from the frame being drawn. Empty bones are a creature that
/// has not been posed yet. The functions are in creature_turn_pose (Creature/CreatureTurnPose.h)
struct CreatureTurnPose
{
	/// The bones as posed by the latest pose and by the one before, which the body is drawn between
	std::vector<glm::mat4> current;
	std::vector<glm::mat4> previous;
	/// The pose kept while the hand holds the creature off the game, which the game reads instead of current
	std::optional<std::vector<glm::mat4>> frozen;
};

} // namespace openblack::ecs::components
