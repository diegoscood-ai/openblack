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

namespace openblack::ecs::components
{

/// The matrix a villager's, an animal's or a creature's model was last placed with, as the game leaves it on the
/// thing: a villager or an animal as its draw in the object draw list places it (ecs::DrawnLivingPlacement: between its
/// turn's two places, turned and sheared on the slope as it is drawn); a creature at its turn's place, turned and at
/// the size it is drawn at, written by its body's preparation every frame. The game's readers of the thing's placement
/// read this one (object_placement::Of), so a turn reads what the frames drawn before it left
/// (docs/wiki/engine/matrices-and-zoomer.md#the-objects-own-placement-in-openblack). The renderer never reads it
struct LastDrawnMatrix
{
	glm::mat4x3 cells {1.0f};
	/// the scale and the y angle it was placed with (a creature's angle is not kept: 0)
	float scale {1.0f};
	float yAngle {0.0f};
};

} // namespace openblack::ecs::components
