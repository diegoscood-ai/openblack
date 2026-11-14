/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// The matrix a thing a particle carries (a tornado's, a vortex's) is drawn with, as the original's carrying particle
/// gives its 3D object every frame: the atom's drawn rows, the scale in them, at its drawn place
/// (docs/wiki/physics/particles.md). Kept apart from the thing's Transform, its game place and scale, which the carrying
/// leaves alone. Only the renderer reads it (ecs::DrawnModel), and the release takes its rows; the state hash does not
struct ParticleDrawnMatrix
{
	glm::mat4 model {1.0f};
};

} // namespace openblack::ecs::components
