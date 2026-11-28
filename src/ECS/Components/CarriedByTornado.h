/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{
/// Something a tornado has picked up, between physics::particle_carried_objects::Take and Release: it is in physics
/// with no body, out of the map cells, not hit and not picked up (a tree's landing reads it), and it follows the
/// particle carrying it until the particle goes. Only the tornado takes objects this way; the vortex flings them.
/// The carrier itself stays with the tornado's particle, so this is only a mark.
struct CarriedByTornado
{
};
} // namespace openblack::ecs::components
