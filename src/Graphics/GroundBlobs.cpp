/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GroundBlobs.h"

#include <glm/geometric.hpp>

namespace openblack::graphics::ground_blobs
{

namespace
{
/// Half a blob's width, either way across the foot: 0.2 along the diagonal (1, 0, -1) and back
constexpr glm::vec3 k_Back(0.14142136f, 0.0f, -0.14142136f);
constexpr glm::vec3 k_Across(-0.14142136f, 0.0f, 0.14142136f);
/// The light's offset, 2 units along the diagonal (1, 0, 1)
constexpr glm::vec3 k_Offset(1.41421356f, 0.0f, 1.41421356f);
/// How far behind the foot a blob starts, as a part of its fall
constexpr float k_Behind = 0.02f;
} // namespace

glm::vec3 Fall(const glm::vec3& landNormal, float scale)
{
	const auto offset = k_Offset * scale;
	return offset - glm::dot(offset, landNormal) * landNormal;
}

Quad MakeQuad(const glm::vec3& foot, const glm::vec3& fall)
{
	return {{foot - k_Behind * fall + k_Back, foot - k_Behind * fall + k_Across, foot + fall + k_Across, foot + fall + k_Back}};
}

std::array<Quad, 2> Feet(const glm::vec3& first, const glm::vec3& second, const glm::vec3& fall)
{
	return {MakeQuad(first, fall + (second - first) * 0.5f), MakeQuad(second, fall + (first - second) * 0.5f)};
}

} // namespace openblack::graphics::ground_blobs
