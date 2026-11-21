/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleControlDraw.h"

#include <cmath>

std::vector<uint32_t> openblack::temple::control_draw::ControlSubMeshes(uint32_t subMeshCount,
                                                                        const std::function<bool(uint32_t)>& isControl)
{
	std::vector<uint32_t> controls;
	for (uint32_t index = 0; index < subMeshCount; ++index)
	{
		if (isControl(index))
		{
			controls.push_back(index);
		}
	}
	return controls;
}

glm::vec3 openblack::temple::control_draw::TimesWhite(glm::vec3 colour)
{
	glm::vec3 multiplied;
	for (glm::length_t i = 0; i < 3; ++i)
	{
		const auto byte = std::lround(colour[i] * 255.0f);
		multiplied[i] = static_cast<float>((byte * 255) >> 8) / 255.0f;
	}
	return multiplied;
}

glm::vec3 openblack::temple::control_draw::HeldButtonOffset(const glm::mat4& frameToMesh)
{
	return glm::vec3(frameToMesh[2]) * -k_HeldButtonSink;
}
