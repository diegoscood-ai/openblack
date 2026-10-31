/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x3.hpp>

#include "ECS/Components/Transform.h"

namespace openblack::ecs::components
{

/// The placement an object's model has, kept apart from its game position (its Transform), as the game keeps it: the
/// three rows and the translation (column 3) of the model's matrix, and the one scale and the y angle it was made with
/// (docs/wiki/engine/matrices-and-zoomer.md#the-objects-own-placement). It is made only where the game makes it again:
/// at the object's creation, when its angles or its scale are set, when it is put somewhere by a script; a move that
/// does not set it again leaves it where it was. Written through ecs::object_placement, read through
/// object_placement::Cells
struct ObjectMatrix
{
	glm::mat4x3 cells {1.0f};
	float scale {1.0f};
	float yAngle {0.0f};
	/// The game position it was made from: while the Transform is still this one, the cells are its placement; a
	/// Transform moved since by a move that makes no placement of its own is read as placed again
	/// (object_placement::Of)
	Transform madeFrom {};
};

} // namespace openblack::ecs::components
