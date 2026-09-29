/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/mat4x4.hpp>

namespace openblack
{
class L3DAnim;
}

namespace openblack::graphics
{
class L3DMesh;

/// The model matrix of every bone of the mesh at `milliseconds` into the clip (the frames interpolated like the
/// original, then each bone put under its parent). A clip for another skeleton leaves the rest pose.
void ComputePose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose);

} // namespace openblack::graphics
