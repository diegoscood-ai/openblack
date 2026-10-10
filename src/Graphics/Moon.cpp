/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Moon.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_BasisScale = 4.0f;
/// The moon leans back a little, about 7.5 degrees
constexpr float k_Tilt = -0.13089970f;
constexpr float k_MeshScale = 0.65f;
constexpr float k_GlowHalfSize = 500.0f;
constexpr float k_GlowUvMinimum = 0.25f;
constexpr float k_GlowUvMaximum = 0.49375f;
} // namespace

std::optional<moon::Placement> moon::Place(float scriptHour)
{
	// An hour of the day is a twelfth of half a turn
	const float theta = scriptHour * std::numbers::pi_v<float> / 12.0f;
	const glm::vec3 offset(4000.0f, 1100.0f * std::cos(theta) - 150.0f, 800.0f * std::sin(theta));
	// It shows for about 4.7 hours either side of midnight
	const float alpha = std::min(200.0f, std::floor(0.5f * offset.y - 110.0f));
	if (alpha <= 0.0f)
	{
		return std::nullopt;
	}
	return Placement {.offset = offset, .alpha = alpha};
}

float moon::Phase(int64_t unixTime)
{
	// Whole days from a new moon, in moon months of about 29.5 days. Each product and difference is rounded to a float,
	// the constants are doubles.
	const auto days = static_cast<int32_t>(unixTime / 86400) - 0x2AD2;
	const auto cycles = static_cast<float>(static_cast<double>(days) * 0.03386318012808897);
	const float fraction = cycles - static_cast<float>(static_cast<int32_t>(cycles));
	const auto rest = static_cast<float>(1.0 - static_cast<double>(fraction));
	return static_cast<float>(static_cast<double>(rest) * 6.2831854820251465);
}

glm::mat3 moon::Basis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position)
{
	// Its face square to the line from the camera, and upright
	const glm::vec3 v(view * glm::vec4(position, 1.0f));
	glm::vec3 n = v;
	if (v != glm::vec3(0.0f))
	{
		n = v * (1.0f / std::sqrt(v.z * v.z + v.y * v.y + v.x * v.x));
	}
	glm::vec3 t(n.z, 0.0f, -n.x);
	if (t != glm::vec3(0.0f))
	{
		t = t * (1.0f / std::sqrt(t.z * t.z + t.y * t.y + t.x * t.x));
	}
	const glm::vec3 u = glm::cross(n, t);
	return glm::mat3(inverseView) * glm::mat3(t, u, n) * k_BasisScale;
}

glm::mat4 moon::Model(const glm::mat3& basis, const glm::vec3& position, float phase)
{
	glm::mat3 rows = basis;
	affine::RotateZ(rows, k_Tilt);
	affine::RotateY(rows, phase + k_Pi);
	glm::mat4 model(rows * k_MeshScale);
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

moon::Glow moon::MakeGlow(const glm::mat3& basis, const glm::vec3& position)
{
	const glm::vec3& r0 = basis[0];
	const glm::vec3& r1 = basis[1];
	return {
	    .corners = {position - (r1 + r0) * k_GlowHalfSize, (r0 * k_GlowHalfSize - r1 * k_GlowHalfSize) + position,
	                (r1 * k_GlowHalfSize - r0 * k_GlowHalfSize) + position, (r1 + r0) * k_GlowHalfSize + position},
	    .uvs = {glm::vec2(k_GlowUvMinimum, k_GlowUvMinimum), glm::vec2(k_GlowUvMaximum, k_GlowUvMinimum),
	            glm::vec2(k_GlowUvMinimum, k_GlowUvMaximum), glm::vec2(k_GlowUvMaximum, k_GlowUvMaximum)},
	};
}

glm::vec3 moon::GlowColour(const glm::vec3& moonColour)
{
	return {moonColour.r / 6.0f, moonColour.g / 5.0f, moonColour.b / 4.0f};
}
