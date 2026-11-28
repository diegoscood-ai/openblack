/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <vector>

#include <L3DFile.h>
#include <glm/vec3.hpp>

namespace openblack::test
{
/// A special point of a mesh: where it is from the middle of the mesh and its turn about the vertical
struct MeshSpecialPoint
{
	glm::vec3 offset {0.0f};
	float yAngle {0.0f};
};

/// The bytes of an L3D file with no sub-mesh and no skin that holds only special points: the header, then its extra
/// metrics block (its size, the count, where the metrics start, then each metric). A metric is the original's matrix,
/// whose rows for a turn of a about the vertical are (c, 0, s), (0, 1, 0), (-s, 0, c), then the point. No geometry, so
/// loading it builds no bgfx buffer
[[nodiscard]] inline std::vector<uint8_t> MeshOfSpecialPoints(const std::vector<MeshSpecialPoint>& points)
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	using Metric = std::array<float, 12>;
	const auto count = static_cast<uint32_t>(points.size());
	const auto metricsAt = static_cast<uint32_t>(sizeof(l3d::L3DHeader) + (3 * sizeof(uint32_t)));
	const std::array<uint32_t, 3> block {static_cast<uint32_t>((3 * sizeof(uint32_t)) + (count * sizeof(Metric))), count,
	                                     metricsAt};
	std::vector<uint8_t> file(metricsAt + (count * sizeof(Metric)));
	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.flags = l3d::L3DMeshFlags::ContainsExtraMetrics;
	header.size = static_cast<uint32_t>(file.size());
	header.submeshOffsetsOffset = k_None;
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	// the extra blocks start right after the header
	header.footprintDataOffset = static_cast<uint32_t>(sizeof(l3d::L3DHeader));
	std::memcpy(file.data(), &header, sizeof(header));
	std::memcpy(file.data() + sizeof(header), block.data(), sizeof(block));
	for (size_t i = 0; i < points.size(); ++i)
	{
		const auto& point = points[i];
		const float c = std::cos(point.yAngle);
		const float s = std::sin(point.yAngle);
		const Metric metric {c, 0.0f, s, 0.0f, 1.0f, 0.0f, -s, 0.0f, c, point.offset.x, point.offset.y, point.offset.z};
		std::memcpy(file.data() + metricsAt + (i * sizeof(Metric)), metric.data(), sizeof(Metric));
	}
	return file;
}
} // namespace openblack::test
