/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureTemplates.h"

// Recognition: the buffer's keypoints against the templates (0x578C20..0x57A880). Only the turn sequence, the first
// direction and the aspect class are compared. Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::magic::gestures
{
/// T1 [0xD064C0] = 21 pi / 128: a "small" turn, which may be absorbed
constexpr float k_SmallTurn = 0.51541936f;
/// T2 [0xD064BC] = 3 pi / 16: the largest accumulated turn error
constexpr float k_MaxError = 0.58904862f;
/// The aspect classes [0x900110] / [0x900114]
constexpr float k_AspectThin = 0.15f;
constexpr float k_AspectWide = 4.0f;

/// GestureSystemResult (g_game +0x250070)
struct Result
{
	Gesture gesture {k_None}; ///< +0
	bool reversed {false};    ///< +4 matched mirrored
	uint8_t start {0};        ///< +8 first matched keypoint
	uint8_t end {0};          ///< +9 last matched keypoint
	uint8_t templateIndex {0}; ///< +0xA GetOffset of the template
};

/// GestureSystemPacketData (0x18 bytes): what an apply sends (the circle's centre and world radius)
struct Packet
{
	Gesture gesture {k_None}; ///< +0
	bool reversed {false};    ///< +4
	glm::vec3 position {0.0f}; ///< +8 world point
	float size {0.0f};        ///< +0x14
};

/// The camera services PacketFromResult needs (LH3D's screen/land functions)
struct Projection
{
	/// fn_005E5620: the land point under a pixel (nullopt off the land)
	std::function<std::optional<glm::vec3>(glm::vec2 pixel)> screenToLand;
	/// Get3DPointFromScreen: the unit direction of the ray through a pixel
	std::function<glm::vec3(glm::vec2 pixel)> rayDirection;
	glm::vec3 cameraPosition {0.0f}; ///< LH3DTech::g_camera
	/// (cos yaw, -sin yaw) of fn_00441E60 in x, z (inf: the camera's right on the ground)
	glm::vec2 yawAxis {1.0f, 0.0f};
};

/// fn_00578E40: (W + 1) / max(1, (H + 1) * screenRatio), screenRatio = screen width / height (0xE85058 / 0xE8505A)
[[nodiscard]] float Aspect(const BoundingBox& box, float screenRatio);

/// BuildFromSystem 0x578C20: the keypoints (flags & 0xB, and the newest sample), then ComputeAspect 0x578EA0
[[nodiscard]] GestureData BuildFromSystem(const GestureSystem& system, float screenRatio);

/// GestureSystemDataList::Match 0x57A050: forward (MatchForward 0x57A1A0), then mirrored if the template allows it
/// (MatchMirror 0x57A3E0); res.templateIndex = index
bool Match(const GestureData& tpl, uint8_t index, const GestureData& input, Result& result, float screenRatio);

/// MatchGesture 0x579F10: every template of that gesture, in file order
bool MatchGesture(const std::vector<GestureData>& list, Gesture gesture, const GestureData& input, Result& result,
                  float screenRatio);

/// fn_0057A5E0: the packet of a result. PositionMode 2 (every template): the land under the centre of the matched
/// samples' box, and size = 1.05 x the world half-width of the box at that distance; other modes: the first matched
/// sample's land point and size 1
[[nodiscard]] Packet PacketFromResult(const std::vector<GestureData>& list, const GestureSystem& system,
                                      const Result& result, const Projection& projection);
} // namespace openblack::magic::gestures
