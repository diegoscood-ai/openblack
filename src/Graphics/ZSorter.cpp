/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ZSorter.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_RainTileScale = -0.0125f; ///< [0x9A3AC0], -1 / 80
} // namespace

float zsorter::Key(const glm::vec3& point, const glm::vec3& camera, SumOrder order) noexcept
{
	// fsub [0xEA1DB8] / [0xEA1DBC] / [0xEA1DC0], each square with fmul, the two faddp and fstp dword [esp]: in float at
	// the original's 24-bit precision, no fused multiply-add
	const float x = point.x - camera.x;
	const float y = point.y - camera.y;
	const float z = point.z - camera.z;
	const float xx = x * x;
	const float yy = y * y;
	const float zz = z * z;
	if (order == SumOrder::XZY)
	{
		const float xz = xx + zz;
		return xz + yy;
	}
	const float xy = xx + yy;
	return xy + zz;
}

uint32_t zsorter::PackRainUser(float x, float z, int32_t alpha) noexcept
{
	// 0x834233 fmul [0x9A3AC0], __ftol (truncates); 0x834248 shl esi, 8; sub; shl 8; 0x834259 sub: in 32-bit integers
	const auto tileZ = static_cast<int32_t>(z * k_RainTileScale);
	const auto tileX = static_cast<int32_t>(x * k_RainTileScale);
	uint32_t user = static_cast<uint32_t>(alpha);
	user = (user << 8u) - static_cast<uint32_t>(tileZ);
	user = (user << 8u) - static_cast<uint32_t>(tileX);
	return user;
}

zsorter::RainUser zsorter::UnpackRainUser(uint32_t user) noexcept
{
	// 0x833F83..0x833FB4: [0xEE9D30] & 0xFF, ([0xEE9D30] >> 8) & 0xFF and the byte [0xEE9D32]
	return {static_cast<int32_t>(user & 0xFFu), static_cast<int32_t>((user >> 8u) & 0xFFu),
	        static_cast<int32_t>((user >> 16u) & 0xFFu)};
}
