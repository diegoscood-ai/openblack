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

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ArgbColour.h"

namespace openblack::graphics::partial_build_cap
{

/// A corner of the cap, in the model's own space
struct CapVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

/// A primitive's cut segments, of both its outer and inner walls, are kept in one list, which stores them while it holds
/// at most this many and only notes that it overflowed after that
inline constexpr size_t k_MostStoredSegments = 1000;

/// The most segments one primitive's cut may have and still be capped: both walls cut as many, and a list past the most
/// stored shows no cap at all
inline constexpr size_t k_MostSegments = (k_MostStoredSegments + 1) / 2;

/// An edge the cut crosses, by the places in the triangle of its corner below the cut and its corner above it
struct CutEdge
{
	uint8_t below;
	uint8_t above;
};

/// The two edges a triangle is cut along, in the order its segment's ends are taken, from which of its corners are at or
/// above the cut; none when none or all of them are
[[nodiscard]] std::optional<std::array<CutEdge, 2>> CutEdgesOf(std::array<bool, 3> above);

/// How far along an edge from its corner below the cut to its corner above it the cut lies. Each corner's distance from
/// the cut is kept on its own side of it, a hair away at least, so that a corner that was put on the wrong side still
/// gives a point on the edge.
[[nodiscard]] float CutShare(float belowHeight, float aboveHeight, float height);

/// A primitive's cut segments, outer walls first and then inner ones, as the game keeps them
template <typename Vertex>
class SegmentList
{
public:
	/// Stores a segment, or only notes the overflow once the list is full
	void Record(const Vertex& start, const Vertex& end)
	{
		if (_segments.size() > k_MostStoredSegments)
		{
			_overflow = true;
			return;
		}
		_segments.push_back({start, end});
	}
	[[nodiscard]] size_t Size() const { return _segments.size(); }
	[[nodiscard]] bool Overflowed() const { return _overflow; }
	[[nodiscard]] const std::array<Vertex, 2>& operator[](size_t i) const { return _segments[i]; }

private:
	std::vector<std::array<Vertex, 2>> _segments;
	bool _overflow {false};
};

/// Whether a cut primitive is capped: its outer walls were cut, its inner walls as many times, and the list did not
/// overflow
[[nodiscard]] bool Capped(size_t outerSegments, size_t innerSegments, bool overflowed);

/// The corners of a segment's piece of cap
enum class CapCorner : uint8_t
{
	OuterStart,
	OuterEnd,
	InnerStart,
	InnerEnd,
};

/// The cap's shade of the object's colour, out of 256: three quarters
inline constexpr uint32_t k_CapShade = 192;

/// The colour of every corner of the cap: the object's colour (its land light, times its tint) at three quarters, (c 3) >> 2
/// in R, G and B, the alpha kept. The cap is not lit by the model light, and its specular is the object's.
[[nodiscard]] constexpr uint32_t CapColour(uint32_t objectColour) noexcept
{
	return argb_colour::ScaleRgbShift8KeepAlpha(objectColour, k_CapShade);
}

/// Whether a primitive takes the snow lying on its object: only an object that takes snow, only a primitive that writes
/// the depth, and never a primitive of a partly built model (the original draws snow only on whole objects)
[[nodiscard]] constexpr bool TakesSnow(bool objectTakesSnow, bool depthWrite, bool partialBuild) noexcept
{
	return objectTakesSnow && depthWrite && !partialBuild;
}

/// A segment's piece of cap as two triangles: outer start, outer end, inner start; then inner end, outer end, inner start
inline constexpr std::array<CapCorner, 6> k_CapQuad {CapCorner::OuterStart, CapCorner::OuterEnd, CapCorner::InnerStart,
                                                     CapCorner::InnerEnd,   CapCorner::OuterEnd, CapCorner::InnerStart};

/// The cap over a building's model cut at a height, where its walls are cut through: each triangle with corners both at
/// or above the height and below it is cut along the two of its edges that cross it, and that segment on the outer wall
/// is joined to the same segment on the inner wall, whose corners stand in from the outer ones across the ground along
/// their normals by the inset before they are cut. Each segment gives the two triangles of k_CapQuad, with the texture
/// coordinates and normals taken along the edges. Nothing when nothing is cut, or when more than the most segments are.
[[nodiscard]] std::vector<CapVertex> Build(std::span<const glm::vec3> positions, std::span<const glm::vec2> uvs,
                                           std::span<const glm::vec3> normals, std::span<const uint16_t> indices, float height,
                                           float inset);

/// Whether the cut draws anything of a primitive: any triangle with a corner below the height, which is drawn cut down to
/// the part below it. Only such a primitive has inner walls and a cap. (The game also leaves them out when all it would
/// draw below the cut is off screen; that changes nothing that can be seen.)
[[nodiscard]] bool DrawsAnythingBelow(std::span<const glm::vec3> positions, std::span<const uint16_t> indices, float height);

} // namespace openblack::graphics::partial_build_cap
