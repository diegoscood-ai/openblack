/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChainRibbon.h"

#include <cstddef>

#include <glm/geometric.hpp>

#include "3D/Billboard.h"

using namespace openblack;
using namespace openblack::graphics;

std::vector<glm::vec3> chain_ribbon::Corners(std::span<const Joint> joints, const glm::vec3& eye)
{
	if (joints.size() < 2)
	{
		return {};
	}
	const auto segments = joints.size() - 1;
	// four corners per segment, (head + side, head - side, tail + side, tail - side), each end widened by its own side
	// vector: normalize(cross(eye - joint, segment)) (the sign of billboard::RibbonSide) x the joint's scale (side x
	// scale / |side|; a zero side stays zero), so the half width is the scale itself
	std::vector<glm::vec3> corners(segments * k_CornersPerSegment);
	for (size_t i = 0; i < segments; ++i)
	{
		const auto& head = joints[i];
		const auto& tail = joints[i + 1];
		const auto along = tail.position - head.position;
		// (eye - joint) x segment, unnormalised as in the original, then side / |side| x scale; only an exactly zero side
		// is left as it is, so a zero-length segment (two joints at one point, the gesture trail of a still hand)
		// collapses to its joint instead of the NaN billboard::RibbonSide's normalize(segment) would give
		const auto sideOf = [&eye, &along](const glm::vec3& joint, float scale) {
			const auto side = glm::cross(eye - joint, along);
			const float length = glm::length(side);
			// the half width (billboard::RibbonHalfWidth): side x the joint's scale / |side|, the same value a sprite
			// takes as its half size
			return length > 0.0f ? side * (billboard::RibbonHalfWidth(scale) / length) : glm::vec3(0.0f);
		};
		const auto sideHead = sideOf(head.position, head.scale);
		const auto sideTail = sideOf(tail.position, tail.scale);
		auto* c = &corners[i * k_CornersPerSegment];
		c[0] = head.position + sideHead;
		c[1] = head.position - sideHead;
		c[2] = tail.position + sideTail;
		c[3] = tail.position - sideTail;
	}
	// where two segments meet, both pairs of corners move to their midpoints, so the strip has no gaps or steps at the
	// joints (the original's other mode, off here, would copy the previous tail instead)
	for (size_t i = 1; i < segments; ++i)
	{
		auto* previous = &corners[(i - 1) * k_CornersPerSegment];
		auto* c = &corners[i * k_CornersPerSegment];
		c[0] = previous[2] = (previous[2] + c[0]) * 0.5f;
		c[1] = previous[3] = (previous[3] + c[1]) * 0.5f;
	}
	return corners;
}
