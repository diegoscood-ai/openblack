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

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureTemplates.h"

// The live sample buffer (GestureSystem, g_game +0x25006C, 0xC98 bytes): the mouse samples and their keypoints, found
// online as the samples arrive (0x57BBC0..0x57C8F0). Wiki: docs/bw1-notes/magic.md, "Gestos".

namespace openblack::magic::gestures
{
/// A buffer sample (0x28 bytes)
struct Sample
{
	float sx {0.0f};              ///< +0x00 mouse x, pixels
	float sy {0.0f};              ///< +0x04 (0)
	float sz {0.0f};              ///< +0x08 mouse y, pixels (down)
	float turn {0.0f};            ///< +0x0C the signed turn at this corner
	uint32_t direction {0};       ///< +0x10 octant of the outgoing segment
	glm::vec3 world {0.0f};       ///< +0x14 the land point under the cursor
	uint32_t flags {0};           ///< +0x20 k_Start, k_Corner, k_Anchor, k_End or 0
	float heading {0.0f};         ///< +0x24 Atan2Positive of the outgoing segment

	static constexpr uint32_t k_Start = 1;
	static constexpr uint32_t k_Corner = 2;
	static constexpr uint32_t k_Anchor = 4; ///< the newest sample after a corner was found
	static constexpr uint32_t k_End = 8;    ///< the newest sample
};

/// {minX, minZ, maxX, maxZ} in pixels
struct BoundingBox
{
	float minX {0.0f};
	float minZ {0.0f};
	float maxX {0.0f};
	float maxZ {0.0f};
};

/// T_corner [0xD064FC] = pi/8 * 3/4 (initialiser 0x57BB00): the smallest turn that makes a corner
constexpr float k_CornerTurn = 0.29452431f;

/// fn_00578890: wrap(b - a): > pi -> -2pi, <= -pi -> +2pi
[[nodiscard]] float WrapDifference(float a, float b);
/// Atan2Positive 0x7DB770: atan2(z, x) in [0, 2pi)
[[nodiscard]] float Atan2Positive(float x, float z);
/// fn_00578700: to the nearest integer, an exact .5 goes down (ftol truncates, +1 only above .5)
[[nodiscard]] int RoundHalfDown(float v);
/// fn_00578730: the heading's octant, +pi/2 (0 = up on the screen), rounded with fn_00578700 (an exact .5 rounds down)
[[nodiscard]] uint32_t Octant(float heading);

class GestureSystem
{
public:
	static constexpr uint8_t k_Size = 80;
	/// 70 samples on the same pixel wipe the buffer (0x57BC90)
	static constexpr uint32_t k_StationaryLimit = 70;

	/// AddSample 0x57BBC0: a new sample at the head (mouse in pixels); then its keypoint work (ProcessNewSample)
	void AddSample(const glm::vec3& world, glm::ivec2 mouse);
	/// fn_0057C8F0: a sample that reuses the newest one's land point (the cursor is off the land); nothing when empty
	void AddSampleAtLastWorld(glm::ivec2 mouse);
	/// count = head = stationary = 0 and the samples zeroed (inlined in many places)
	void Clear();

	[[nodiscard]] uint8_t Count() const { return _count; }
	[[nodiscard]] uint8_t Head() const { return _head; }
	/// The physical slot of logical sample i (0 = the oldest): (head - count + i + 80) % 80, or 0 for i > count
	[[nodiscard]] uint8_t Physical(int i) const;
	[[nodiscard]] Sample& At(int i) { return _samples[Physical(i)]; }
	[[nodiscard]] const Sample& At(int i) const { return _samples[Physical(i)]; }

	/// fn_005789D0: the box of samples s..b-1 (from the first one; samples at (0,0,0) are skipped after it)
	[[nodiscard]] BoundingBox Box(int s, int b) const;
	/// fn_005788D0: the raw sample indices of keypoints number `start` and `end` (keypoints: flags & 0xB, or the last)
	void KeypointIndices(int start, int end, int& first, int& last) const;

private:
	/// ProcessNewSample 0x57C3F0
	void ProcessNewSample(int i);
	/// fn_0057BE10: the last j in [1, i-1] with flags, else 0
	[[nodiscard]] int PrevNonZero(int i) const;
	/// fn_0057C1A0: the last j in [1, i-1] that is a corner, else 0 (the start)
	[[nodiscard]] int PrevCorner(int i) const;
	/// fn_0057BE70: the last j in [1, i-1] with flags, or with a non-zero position far from sample i, else 0
	[[nodiscard]] int PrevAnchor(int i) const;
	/// FindCorner 0x57BFE0 (k = the previous keypoint)
	[[nodiscard]] int FindCorner(int k, int i) const;
	/// MergeOrReject 0x57C200: 0 = keep c as a new corner, 1 = merged into the previous one (or the start)
	[[nodiscard]] int MergeOrReject(int c, int i);
	/// LongEnough fn_0057C630: segment a->b of delta (dx, dz)
	[[nodiscard]] bool LongEnough(int a, int b, float dx, float dz) const;
	/// UpdateHeading 0x57C710 + fn_0057C820: the outgoing heading, octant and turn of the corner before i
	void UpdateHeading(int i);
	/// fn_0057BF60: wrap(Atan2P(c - b) - Atan2P(b - a)) on the screen positions
	[[nodiscard]] static float Turn(const Sample& a, const Sample& b, const Sample& c);
	/// fn_0057C590 / fn_0057C5C0: 4 pixels or more apart on either axis
	[[nodiscard]] static bool Far(const Sample& a, const Sample& b);

	std::array<Sample, k_Size> _samples {}; ///< +0x008
	uint8_t _count {0};                     ///< +0xC88
	uint32_t _stationary {0};               ///< +0xC8C
	uint8_t _head {0};                      ///< +0xC90
};
} // namespace openblack::magic::gestures
