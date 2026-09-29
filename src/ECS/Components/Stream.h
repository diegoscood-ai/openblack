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

namespace openblack::ecs::components
{

/// A river (GStream, GameThing type 0x47, Stream.cpp): its points in script order (CREATE_STREAM_POINT appends at the
/// tail, 0x733B90), each at the ground altitude (the handler 0x717550 sets y). The river runs p[i] -> p[i + 1].
struct Stream
{
	using Id = int;

	Id id;
	std::vector<glm::vec3> points;
};

/// One landscape footprint of a river segment (CreateRiver 0x7341E0 adds two per segment): data\river2.l3d, the
/// river bed, blended into the land colour like a building footprint; data\river.l3d (channel = true), whose alpha
/// lowers the land's alpha (min) so the sea drawn before the land shows through as the water.
struct StreamFootprint
{
	bool channel;
};

} // namespace openblack::ecs::components
