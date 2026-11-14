/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFrozenSheen.h"

#include <cmath>

#include "Common/ModelInverseSquareRoot.h"
#include "Graphics/ArgbColour.h"

namespace openblack::creature_frozen_sheen
{

namespace
{
constexpr float k_WiggleSpeed = 2.0f;
constexpr float k_PerMillisecond = 0.001f;
constexpr float k_WiggleY = 1.1f;
constexpr float k_WiggleZ = 1.3f;
constexpr float k_WiggleAmplitude = 0.02f;
/// Two pi as a float, widened for the wrap round
constexpr double k_TwoPi = static_cast<double>(6.2831855f);
/// The face's coordinate scale: 255 / 512
constexpr float k_UvScale = 0.498046875f;

float Wrap(float angle)
{
	return static_cast<float>(std::fmod(static_cast<double>(angle), k_TwoPi));
}
} // namespace

uint8_t FrozenLevel(float frozen)
{
	return static_cast<uint8_t>(static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(frozen * 255.0f))) & 0xFFu);
}

uint8_t SheenAlpha(float frozen)
{
	return static_cast<uint8_t>((static_cast<uint32_t>(FrozenLevel(frozen)) * 0xFFu) >> 8u);
}

uint32_t SheenColour(float frozen, std::optional<uint32_t> pieceColour)
{
	// The object's colour, opaque, times white at the frozen level; a body's ice is then made white again
	const uint32_t level = FrozenLevel(frozen);
	const uint32_t colour =
	    argb_colour::MultiplyArgbShift8((level << 24u) | 0x00FFFFFFu, pieceColour.value_or(0xFFFFFFFFu) | 0xFF000000u);
	return pieceColour.has_value() ? colour : colour | 0x00FFFFFFu;
}

glm::vec3 StepWiggle(glm::vec3 angles, int32_t frameGameMs)
{
	const float step = static_cast<float>(frameGameMs) * k_WiggleSpeed * k_PerMillisecond;
	const float stepY = step * k_WiggleY;
	const float stepZ = step * k_WiggleZ;
	return {Wrap(angles.x + step), Wrap(angles.y + stepY), Wrap(angles.z + stepZ)};
}

Rows WiggleTurn(glm::vec3 angles)
{
	// The first sine is kept wider than a float, the other two are stored as floats first, as the original keeps them
	const float z = static_cast<float>(std::sin(static_cast<double>(angles.z))) * k_WiggleAmplitude;
	const float x = static_cast<float>(std::sin(static_cast<double>(angles.y))) * k_WiggleAmplitude;
	const auto y = static_cast<float>(k_WiggleAmplitude * std::sin(static_cast<double>(angles.x)));

	// The turn about y's sine and cosine stay wider than a float; each product is a float
	const double cosY = std::cos(static_cast<double>(y));
	const double sinY = std::sin(static_cast<double>(y));
	const auto cosX = static_cast<float>(std::cos(static_cast<double>(x)));
	const auto sinX = static_cast<float>(std::sin(static_cast<double>(x)));
	const auto cosZ = static_cast<float>(std::cos(static_cast<double>(z)));
	const auto sinZ = static_cast<float>(std::sin(static_cast<double>(z)));
	const auto f = [](double value) { return static_cast<float>(value); };

	const float negSinY = f(-sinY);
	const float sinYsinX = f(sinY * sinX);
	const float cosYsinX = f(cosY * sinX);
	const float cosYsinZ = f(cosY * sinZ);
	return {{
	    {f(sinYsinX * sinZ) + f(cosY * cosZ), sinYsinX * cosZ - cosYsinZ, f(sinY * cosX)},
	    {cosX * sinZ, cosX * cosZ, 0.0f - sinX},
	    {negSinY * cosZ + cosYsinX * sinZ, cosYsinX * cosZ - negSinY * sinZ, f(cosY * cosX)},
	}};
}

Axes FaceAxes(const Rows& wiggle, float projectionX, float projectionY)
{
	return {
	    .x = {wiggle[0].x / projectionX, wiggle[0].y / projectionY, wiggle[0].z},
	    .y = {wiggle[1].x / projectionX, wiggle[1].y / projectionY, wiggle[1].z},
	};
}

glm::vec3 FaceNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
	const glm::vec3 first = a - b;
	const glm::vec3 second = b - c;
	glm::vec3 normal {
	    first.y * second.z - first.z * second.y,
	    first.z * second.x - first.x * second.z,
	    first.x * second.y - first.y * second.x,
	};
	const float lengthSquared = (normal.x * normal.x + normal.y * normal.y) + normal.z * normal.z;
	const float inverse = game_math::ModelInverseSquareRoot(lengthSquared);
	return {inverse * normal.x, inverse * normal.y, inverse * normal.z};
}

glm::vec3 AwayFromEye(glm::vec3 normal, glm::vec3 corner)
{
	const float facing = normal.x * corner.x + normal.y * corner.y + normal.z * corner.z;
	return facing < 0.0f ? -normal : normal;
}

glm::vec2 FaceUv(glm::vec3 normal, const Axes& axes)
{
	const auto along = [&](const glm::vec3& axis) {
		return ((axis.z * normal.z + axis.y * normal.y) + axis.x * normal.x + 1.0f) * k_UvScale;
	};
	return {along(axes.x), along(axes.y)};
}

} // namespace openblack::creature_frozen_sheen
