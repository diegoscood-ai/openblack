/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Input/HandDemo.h"

/// Where a recorded hand demo acts when it is played on the testbed's flat land: its presses, the camera and mouse they
/// were made with, and the point of the plane the hand's ray then meets, so that a scenario can put objects there.
/// Pure: the camera's lens is given, not read from the game.
namespace openblack::testbed_demo
{

/// A press of the demo: a grab or an action button going down, with the mouse of the last mouse move before it and the
/// camera of that move
struct Press
{
	/// Its record, counted from the first record of the demo
	size_t record {0};
	/// 1 grab down or 3 action down
	uint32_t message {0};
	/// The mouse, normalised 0..1 from the top left corner of the screen
	glm::vec2 mouse {0.5f};
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// The press's time on the demo's clock, in milliseconds
	uint32_t timeMs {0};
};

/// Every press of the demo with a mouse move before it, in order
[[nodiscard]] std::vector<Press> Presses(std::span<const hand_demo::Record> records);
/// The n-th of them, from 0; nothing when the demo has fewer
[[nodiscard]] std::optional<Press> NthPress(std::span<const hand_demo::Record> records, size_t n);

/// The camera's lens as the game sets it: the horizontal field of view in degrees and the screen's width over its height.
/// The clip planes do not move the ray the game casts from the eye, so they are not needed.
struct Lens
{
	float xFovDegrees {70.0f};
	float aspect {1.0f};
};

/// A ray from the eye
struct Ray
{
	glm::vec3 origin {0.0f};
	/// Of length 1, or zero when the camera looks nowhere
	glm::vec3 direction {0.0f};
};

/// The ray from the eye through a point of the screen (0..1 from the top left corner) of a camera at `eye` looking at
/// `focus`, built as the game builds the hand's ray: from the camera's axes, its horizontal field of view and its aspect
[[nodiscard]] Ray RayThroughScreen(glm::vec3 eye, glm::vec3 focus, glm::vec2 screenPoint, Lens lens);

/// The demo's mouse as the game hands it to the hand: rounded to the nearest pixel of a screen of `screenSize`, then
/// back to a fraction of the screen
[[nodiscard]] glm::vec2 RoundToPixel(glm::vec2 mouse, glm::ivec2 screenSize);

/// Where that ray meets the level plane at `height`; nothing when the eye is not above the plane or the ray does not
/// come down to it
[[nodiscard]] std::optional<glm::vec3> PointOnPlane(glm::vec3 eye, glm::vec3 focus, glm::vec2 screenPoint, Lens lens,
                                                    float height);
/// The same for a press
[[nodiscard]] std::optional<glm::vec3> PointOnPlane(const Press& press, Lens lens, float height);

/// The height of the flat land's plane at `altitude`, in units of the map
[[nodiscard]] float PlaneHeight(uint8_t altitude);

/// How far the plane is kept under the demo's lowest eye, in units of the map
constexpr float k_EyeClearance = 2.0f;
/// The highest altitude of the flat land's plane, no higher than its usual one, that lies at least `clearance` under
/// every eye of the demo, so that a demo recorded with the camera close to the ground can be played over it. Nothing
/// when the demo has no records or its lowest eye is too low for a plane above sea level.
[[nodiscard]] std::optional<uint8_t> PlaneAltitudeUnder(std::span<const hand_demo::Record> records,
                                                        float clearance = k_EyeClearance);

} // namespace openblack::testbed_demo
