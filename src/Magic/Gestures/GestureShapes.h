/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureTemplates.h"

// The ideal drawing of each gesture (PSysGesture.cpp: Data\Symbols\PathSymbol<n>.cam) that the "recognised" sparkles
// settle on, and the arc-length paths they move along (GJPath 0x687BE0..0x687E60). Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic::gestures
{
/// A polyline with its cumulative length (GJPath: points of 16 bytes, xyz and the distance so far)
class Path
{
public:
	void Clear() { _points.clear(); _distances.clear(); }
	void Add(const glm::vec3& point);
	[[nodiscard]] size_t Size() const { return _points.size(); }
	[[nodiscard]] const glm::vec3& operator[](size_t i) const { return _points[i]; }
	[[nodiscard]] glm::vec3& operator[](size_t i) { return _points[i]; }
	/// fn_00687CB0: the distances again (after the points moved)
	void Measure();
	/// fn_00687C80: the whole length
	[[nodiscard]] float Length() const { return _distances.empty() ? 0.0f : _distances.back(); }
	/// fn_00687E30: the point at t (0..1) of the length, linear between the points
	[[nodiscard]] glm::vec3 At(float t) const;

private:
	std::vector<glm::vec3> _points;
	std::vector<float> _distances;
};

/// fn_0068C650 / fn_0068C340 (cached per gesture, 0xD4E760): PathSymbol<gesture>.cam, or CIRCLE's (4) when there is no
/// file. Its points (x + 100) / 200, (100 - z) / 200 in 0..1, resampled into as many points evenly along the length.
struct Shape
{
	std::vector<glm::vec3> points; ///< +0 / +4 (x, 0, z)
	float length {0.0f};           ///< +8
	float aspect {1.0f};           ///< +0xC (maxX - minX) / (maxZ - minZ)
	float maxX {0.0f};             ///< +0x10
	float minX {0.0f};             ///< +0x14
	float maxZ {0.0f};             ///< +0x18
	float minZ {0.0f};             ///< +0x1C
};
[[nodiscard]] const Shape& ShapeOf(Gesture gesture);
/// Parses a .cam file's points (u32 file size, u32 ?, u32 count, count x 24 bytes {x, y, z, 3 floats}); false if short
bool ParseShape(const std::vector<uint8_t>& bytes, Shape& shape);

/// The PSysGesture record fn_00689790 hands to UR_GesturingRecognised (0x48 bytes, the list at 0xD4EB10)
struct RecognisedGesture
{
	Path stroke;              ///< +0x04: the land points under the drawn stroke
	Path ideal;               ///< +0x20: the gesture's shape laid on the land, resampled to as many points
	glm::vec3 handPosition {0.0f}; ///< +0x3C (status +0xC8)
	bool fromInterface {true};     ///< +0x38 the status (a FakeGestureOnLandscape has none)
};
} // namespace openblack::magic::gestures
