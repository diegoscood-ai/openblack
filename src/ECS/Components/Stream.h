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

#include <vector>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A river: its points in script order (CREATE_STREAM_POINT appends at the tail), each at the ground altitude (the
/// script handler sets y). The river runs p[i] -> p[i + 1].
struct Stream
{
	using Id = int;

	Id id;
	std::vector<glm::vec3> points;
};

/// One landscape footprint of a river segment (two per segment): data\river2.l3d, the
/// river bed, blended into the land colour like a building footprint; data\river.l3d (channel = true), whose alpha
/// lowers the land's alpha (min) so the sea drawn before the land shows through as the water.
struct StreamFootprint
{
	bool channel;
	/// Its place in the order the land's footprints were made: each segment's channel, then its bed (the prints step
	/// walks them the newest first)
	uint32_t sequence {0};
};

} // namespace openblack::ecs::components
