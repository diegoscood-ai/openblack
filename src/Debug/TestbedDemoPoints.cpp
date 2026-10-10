/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedDemoPoints.h"

#include <cmath>

#include <algorithm>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

#include "3D/FlatLand.h"
#include "3D/LandNormal.h"

using namespace openblack;

namespace
{
constexpr uint32_t k_MouseMove = 0;
constexpr uint32_t k_GrabDown = 1;
constexpr uint32_t k_ActionDown = 3;
} // namespace

std::vector<testbed_demo::Press> testbed_demo::Presses(std::span<const hand_demo::Record> records)
{
	std::vector<Press> presses;
	std::optional<size_t> lastMove;
	for (size_t i = 0; i < records.size(); ++i)
	{
		const auto& record = records[i];
		if (record.message == k_MouseMove)
		{
			lastMove = i;
		}
		else if ((record.message == k_GrabDown || record.message == k_ActionDown) && lastMove.has_value())
		{
			const auto& move = records[*lastMove];
			presses.push_back({.record = i,
			                   .message = record.message,
			                   .mouse = move.mouse,
			                   .eye = move.eye,
			                   .focus = move.focus,
			                   .timeMs = record.timeMs});
		}
	}
	return presses;
}

std::optional<testbed_demo::Press> testbed_demo::NthPress(std::span<const hand_demo::Record> records, size_t n)
{
	const auto presses = Presses(records);
	if (n >= presses.size())
	{
		return std::nullopt;
	}
	return presses[n];
}

testbed_demo::Ray testbed_demo::RayThroughScreen(glm::vec3 eye, glm::vec3 focus, glm::vec2 screenPoint, Lens lens)
{
	// The screen point on a plane one unit in front of the eye, across the horizontal field of view in x and the same
	// over the aspect in y, then along the camera's right, up and forward axes, the rows of its view
	const float tanHalf = std::tan(glm::radians(lens.xFovDegrees) * 0.5f);
	const float x = ((screenPoint.x - 0.5f) * 2.0f) * tanHalf;
	const float y = ((0.5f - screenPoint.y) * 2.0f) * (tanHalf / lens.aspect);
	const glm::mat4 view = glm::lookAt(eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::vec3 right(view[0][0], view[1][0], view[2][0]);
	const glm::vec3 up(view[0][1], view[1][1], view[2][1]);
	const glm::vec3 forward(view[0][2], view[1][2], view[2][2]);
	// Summed and normalised in the same order as the game's ray
	auto direction = forward + up * y + right * x;
	if (direction != glm::vec3(0.0f))
	{
		const float length = std::sqrt(direction.z * direction.z + direction.y * direction.y + direction.x * direction.x);
		direction *= 1.0f / length;
	}
	return {.origin = eye, .direction = direction};
}

glm::vec2 testbed_demo::RoundToPixel(glm::vec2 mouse, glm::ivec2 screenSize)
{
	const auto size = glm::vec2(screenSize);
	return glm::vec2(glm::ivec2(size * mouse + 0.5f)) / size;
}

std::optional<glm::vec3> testbed_demo::PointOnPlane(glm::vec3 eye, glm::vec3 focus, glm::vec2 screenPoint, Lens lens,
                                                    float height)
{
	if (!(eye.y > height))
	{
		return std::nullopt;
	}
	const auto ray = RayThroughScreen(eye, focus, screenPoint, lens);
	// A camera looking straight up or down has no axes, and a ray level or rising never comes down to the plane. A ray
	// within rounding of level counts as level: it would meet the plane beyond the map
	constexpr float k_LevelSlope = 1.0e-4f;
	const bool finite = std::isfinite(ray.direction.x) && std::isfinite(ray.direction.y) && std::isfinite(ray.direction.z);
	if (!finite || !(ray.direction.y < -k_LevelSlope))
	{
		return std::nullopt;
	}
	const float distance = (height - eye.y) / ray.direction.y;
	auto point = ray.origin + ray.direction * distance;
	point.y = height;
	return point;
}

std::optional<glm::vec3> testbed_demo::PointOnPlane(const Press& press, Lens lens, float height)
{
	return PointOnPlane(press.eye, press.focus, press.mouse, lens, height);
}

float testbed_demo::PlaneHeight(uint8_t altitude)
{
	return static_cast<float>(altitude) * land_normal::k_HeightUnit;
}

std::optional<uint8_t> testbed_demo::PlaneAltitudeUnder(std::span<const hand_demo::Record> records, float clearance)
{
	if (records.empty())
	{
		return std::nullopt;
	}
	const auto lowest = std::ranges::min(records, {}, [](const hand_demo::Record& record) { return record.eye.y; }).eye.y;
	const float highest = std::floor((lowest - clearance) / land_normal::k_HeightUnit);
	if (!(highest > static_cast<float>(flat_land::k_SeaLevelAltitude)))
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(std::min(highest, static_cast<float>(flat_land::k_Altitude)));
}
