/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpecialLight.h"

#include <algorithm>

namespace openblack::graphics::special_light
{
namespace
{
/// -pi / 10000, as the game keeps it
constexpr float k_TurnPerMs = -0.000314159261f;
} // namespace

float TextureTurn(int32_t engineMs)
{
	return static_cast<float>(engineMs) * k_TurnPerMs;
}

glm::vec2 TurnedUv(glm::vec2 uv, float cosTurn, float sinTurn)
{
	const float across = uv.x - 0.5f;
	const float down = uv.y - 0.5f;
	return {(across * cosTurn) + (down * sinTurn) + 0.5f, (across * sinTurn) - (down * cosTurn) + 0.5f};
}

std::optional<uint16_t> SolidBone(std::span<const l3d::L3DVertexGroup> groups)
{
	if (groups.size() < 2)
	{
		return std::nullopt;
	}
	const auto bone = groups[1].boneIndex;
	const auto sharing = std::ranges::count(groups, bone, &l3d::L3DVertexGroup::boneIndex);
	if (sharing != 1)
	{
		return std::nullopt;
	}
	return bone;
}

} // namespace openblack::graphics::special_light
