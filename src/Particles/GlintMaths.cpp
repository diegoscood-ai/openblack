/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GlintMaths.h"

#include <cmath>
#include <cstddef>

using namespace openblack::particles;

namespace
{
/// 2 pi as the game stores it, a float
constexpr float k_TwoPi = 6.2831855f;
} // namespace

float maths::GlintRate(int maxAtoms, float ageZeroSize)
{
	return static_cast<float>(maxAtoms) / ageZeroSize;
}

float maths::GlintPulse(float age, float speed, float magnitude)
{
	const float pulse = std::cos(age * speed * k_TwoPi) * magnitude * 0.5f + 1.0f;
	return pulse >= 0.0f ? pulse : 0.0f;
}

float maths::GlintSize(float age, float ageMaxSize, float ageZeroSize)
{
	// an age that does not compare counts as still growing
	const float size = age >= ageMaxSize ? 1.0f - ((age - ageMaxSize) / (ageZeroSize - ageMaxSize)) : age / ageMaxSize;
	if (!(size > 0.0f))
	{
		return 0.0f;
	}
	return size < 1.0f ? size : 1.0f;
}

uint32_t maths::GlintPointCount(std::span<const GlintModelPart> parts)
{
	uint32_t count = 0;
	for (const auto& part : parts)
	{
		for (const auto& range : part.ranges)
		{
			count += range.second;
		}
	}
	return count;
}

std::optional<glm::vec3> maths::GlintLocalPoint(std::span<const GlintModelPart> parts, uint32_t index)
{
	uint32_t remaining = index;
	for (const auto& part : parts)
	{
		for (const auto& [first, count] : part.ranges)
		{
			if (remaining < count)
			{
				const auto vertex = static_cast<size_t>(first) + remaining;
				if (vertex >= part.positions.size())
				{
					return std::nullopt;
				}
				return part.positions[vertex];
			}
			remaining -= count;
		}
	}
	return std::nullopt;
}

glm::vec3 maths::GlintThroughModel(const glm::mat4& model, const glm::vec3& point)
{
	glm::vec3 out(0.0f);
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		out[axis] = (((point.z * model[2][axis]) + (point.y * model[1][axis])) + (point.x * model[0][axis])) + model[3][axis];
	}
	return out;
}
