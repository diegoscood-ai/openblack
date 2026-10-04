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

namespace
{
/// LH3DAnim::GetPose / fn_00839F10: W = L * W(parent) with row vectors, so Wparent * L here (bones come after their
/// parents)
void PutUnderParents(const L3DMesh& mesh, const std::vector<glm::mat4>& local, std::vector<glm::mat4>& pose)
{
	const auto& parents = mesh.GetBoneParents();
	for (size_t i = 0; i < local.size(); ++i)
	{
		pose[i] = parents[i] != std::numeric_limits<uint32_t>::max() ? pose[parents[i]] * local[i] : local[i];
	}
}
} // namespace

void ComputePose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, std::vector<glm::mat4>& pose)
{
	const auto& rest = mesh.GetBoneMatrices();
	std::vector<glm::mat4> local;
	clip.SampleLocal(static_cast<int32_t>(std::max(0.0f, milliseconds)), local);
	pose = rest;
	if (local.size() != rest.size())
	{
		return;
	}
	PutUnderParents(mesh, local, pose);
}

void ComputeBlendedPose(const L3DMesh& mesh, const L3DAnim& clip, float milliseconds, const L3DAnim& oldClip,
                        float oldMilliseconds, float oldWeight, std::vector<glm::mat4>& pose)
{
	const auto& rest = mesh.GetBoneMatrices();
	std::vector<glm::mat4> local;
	std::vector<glm::mat4> old;
	clip.SampleLocal(static_cast<int32_t>(std::max(0.0f, milliseconds)), local, true);
	oldClip.SampleLocal(static_cast<int32_t>(std::max(0.0f, oldMilliseconds)), old, true);
	pose = rest;
	if (local.size() != rest.size() || old.size() != rest.size())
	{
		return;
	}
	// 0x8259B6: 1 - [+0x90] (fld 1, fsub)
	const float weight = 1.0f - oldWeight;
	for (size_t i = 0; i < local.size(); ++i)
	{
		// the 12 floats of the LHMatrix (glm's 3 rows of each of the 4 columns); the 4th row stays (0, 0, 0, 1)
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				// one operation per statement (no contraction), as the FPU at 24 bits rounds each
				const float stored = local[i][column][row] * weight; // 0x839C9A, store != 0
				const float added = old[i][column][row] * oldWeight; // 0x839D91
				local[i][column][row] = added + stored;              // 0x839E6B fadd [edi]
			}
		}
	}
	PutUnderParents(mesh, local, pose);
}

} // namespace openblack::graphics
