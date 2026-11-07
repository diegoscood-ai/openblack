/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PartialBuildCap.h"

#include <algorithm>
#include <array>

namespace openblack::graphics::partial_build_cap
{

namespace
{
/// The least distance from the cut a corner below it keeps, and the most a corner above it does
constexpr float k_LeastBelow = 0.0001f;
constexpr float k_MostAbove = -0.0001f;

/// Where an edge from its corner below the cut to its corner above it crosses the cut, every attribute taken that far along
CapVertex CutPoint(const CapVertex& below, const CapVertex& above, float height)
{
	const float t = CutShare(below.position.y, above.position.y, height);
	const float s = 1.0f - t;
	return {
	    .position = above.position * t + below.position * s,
	    .uv = above.uv * t + below.uv * s,
	    .normal = above.normal * t + below.normal * s,
	};
}

/// The same corner on the inner wall: moved in across the ground along its normal, its height kept
CapVertex Inner(const CapVertex& outer, float inset)
{
	auto inner = outer;
	inner.position.x -= outer.normal.x * inset;
	inner.position.z -= outer.normal.z * inset;
	return inner;
}

/// One wall's cut of a triangle, recorded into the list
void RecordCut(const std::array<CapVertex, 3>& corners, const std::array<CutEdge, 2>& edges, float height,
               SegmentList<CapVertex>& segments)
{
	const auto cut = [&](const CutEdge& edge) { return CutPoint(corners.at(edge.below), corners.at(edge.above), height); };
	segments.Record(cut(edges[0]), cut(edges[1]));
}
} // namespace

std::optional<std::array<CutEdge, 2>> CutEdgesOf(std::array<bool, 3> above)
{
	constexpr uint8_t k_A = 0;
	constexpr uint8_t k_B = 1;
	constexpr uint8_t k_C = 2;
	using Edges = std::array<CutEdge, 2>;
	if (above[0])
	{
		if (above[1])
		{
			if (above[2])
			{
				return std::nullopt;
			}
			return Edges {{{k_C, k_A}, {k_C, k_B}}};
		}
		return above[2] ? Edges {{{k_B, k_A}, {k_B, k_C}}} : Edges {{{k_B, k_A}, {k_C, k_A}}};
	}
	if (above[1])
	{
		return above[2] ? Edges {{{k_A, k_B}, {k_A, k_C}}} : Edges {{{k_A, k_B}, {k_C, k_B}}};
	}
	if (above[2])
	{
		return Edges {{{k_A, k_C}, {k_B, k_C}}};
	}
	return std::nullopt;
}

float CutShare(float belowHeight, float aboveHeight, float height)
{
	float below = height - belowHeight;
	if (below < 0.0f)
	{
		below = k_LeastBelow;
	}
	float above = height - aboveHeight;
	if (above > 0.0f)
	{
		above = k_MostAbove;
	}
	return below / (below - above);
}

bool Capped(size_t outerSegments, size_t innerSegments, bool overflowed)
{
	return outerSegments != 0 && innerSegments == outerSegments && !overflowed;
}

bool DrawsAnythingBelow(std::span<const glm::vec3> positions, std::span<const uint16_t> indices, float height)
{
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const auto below = [&](size_t corner) {
			const auto index = indices[i + corner];
			return index < positions.size() && positions[index].y < height;
		};
		if (below(0) || below(1) || below(2))
		{
			return true;
		}
	}
	return false;
}

std::vector<CapVertex> Build(std::span<const glm::vec3> positions, std::span<const glm::vec2> uvs,
                             std::span<const glm::vec3> normals, std::span<const uint16_t> indices, float height, float inset)
{
	struct CutTriangle
	{
		std::array<CapVertex, 3> corners;
		std::array<CutEdge, 2> edges;
	};
	std::vector<CutTriangle> cut;
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		CutTriangle triangle {};
		std::array<bool, 3> above {};
		bool valid = true;
		for (size_t c = 0; c < triangle.corners.size(); ++c)
		{
			const auto index = indices[i + c];
			if (index >= positions.size() || index >= uvs.size() || index >= normals.size())
			{
				valid = false;
				break;
			}
			triangle.corners.at(c) = {.position = positions[index], .uv = uvs[index], .normal = normals[index]};
			// A corner at the height counts as above it
			above.at(c) = positions[index].y >= height;
		}
		if (!valid)
		{
			continue;
		}
		if (const auto edges = CutEdgesOf(above); edges.has_value())
		{
			triangle.edges = *edges;
			cut.push_back(triangle);
		}
	}
	// The outer walls' segments, then the inner walls', whose corners are moved in before they are cut
	SegmentList<CapVertex> segments;
	for (const auto& triangle : cut)
	{
		RecordCut(triangle.corners, triangle.edges, height, segments);
	}
	const size_t outer = segments.Size();
	for (const auto& triangle : cut)
	{
		std::array<CapVertex, 3> inner {};
		std::ranges::transform(triangle.corners, inner.begin(),
		                       [inset](const CapVertex& corner) { return Inner(corner, inset); });
		RecordCut(inner, triangle.edges, height, segments);
	}
	if (!Capped(outer, segments.Size() - outer, segments.Overflowed()))
	{
		return {};
	}
	std::vector<CapVertex> vertices;
	vertices.reserve(outer * k_CapQuad.size());
	for (size_t s = 0; s < outer; ++s)
	{
		for (const auto corner : k_CapQuad)
		{
			switch (corner)
			{
			case CapCorner::OuterStart:
				vertices.push_back(segments[s][0]);
				break;
			case CapCorner::OuterEnd:
				vertices.push_back(segments[s][1]);
				break;
			case CapCorner::InnerStart:
				vertices.push_back(segments[outer + s][0]);
				break;
			case CapCorner::InnerEnd:
				vertices.push_back(segments[outer + s][1]);
				break;
			}
		}
	}
	return vertices;
}

} // namespace openblack::graphics::partial_build_cap
