/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BonePalette.h"

#include <algorithm>

#include <bgfx/defines.h>

namespace openblack::graphics::bone_palette
{

uint32_t Append(std::vector<glm::vec4>& texels, std::span<const glm::mat4> bones)
{
	const auto first = static_cast<uint32_t>(texels.size() / k_TexelsPerBone);
	texels.reserve(texels.size() + (bones.size() * k_TexelsPerBone));
	for (const auto& bone : bones)
	{
		// glm keeps columns: each texel is one of them, as it is uploaded
		for (int column = 0; column < static_cast<int>(k_TexelsPerBone); ++column)
		{
			texels.push_back(bone[column]);
		}
	}
	return first;
}

uint16_t RowsFor(std::size_t texels) noexcept
{
	return static_cast<uint16_t>(std::max<std::size_t>(1, (texels + k_Width - 1) / k_Width));
}

glm::mat4 BoneAt(std::span<const glm::vec4> texels, uint32_t bone)
{
	glm::mat4 matrix(1.0f);
	for (int column = 0; column < static_cast<int>(k_TexelsPerBone); ++column)
	{
		matrix[column] = texels[(bone * k_TexelsPerBone) + static_cast<std::size_t>(column)];
	}
	return matrix;
}

void SetFirstBone(glm::mat4& model, uint32_t bone) noexcept
{
	model[2][3] = static_cast<float>(bone);
}

uint32_t FirstBone(const glm::mat4& model) noexcept
{
	return static_cast<uint32_t>(model[2][3] + 0.5f);
}

bool DrawsInAnyOrder(uint64_t state) noexcept
{
	const auto depthTest = state & BGFX_STATE_DEPTH_TEST_MASK;
	const bool ordering = depthTest == BGFX_STATE_DEPTH_TEST_LESS || depthTest == BGFX_STATE_DEPTH_TEST_LEQUAL ||
	                      depthTest == BGFX_STATE_DEPTH_TEST_GREATER || depthTest == BGFX_STATE_DEPTH_TEST_GEQUAL;
	return ordering && (state & BGFX_STATE_WRITE_Z) != 0 && (state & BGFX_STATE_BLEND_MASK) == 0 &&
	       (state & BGFX_STATE_BLEND_EQUATION_MASK) == 0 && (state & BGFX_STATE_BLEND_INDEPENDENT) == 0;
}

} // namespace openblack::graphics::bone_palette
