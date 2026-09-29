/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkeletalPose.h"

#include <algorithm>
#include <limits>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"

namespace openblack::graphics
{

void ComputePose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose)
{
	const auto& rest = mesh.GetBoneMatrices();
	const auto& parents = mesh.GetBoneParents();
	std::vector<glm::mat4> local;
	clip.SampleLocal(static_cast<int32_t>(std::max(0.0f, milliseconds)), local);
	pose = rest;
	if (local.size() != rest.size())
	{
		return;
	}
	// LH3DAnim::GetPose: W = L * W(parent) with row vectors, so Wparent * L here (bones come after their parents)
	for (size_t i = 0; i < local.size(); ++i)
	{
		pose[i] = parents[i] != std::numeric_limits<uint32_t>::max() ? pose[parents[i]] * local[i] : local[i];
	}
}

} // namespace openblack::graphics
