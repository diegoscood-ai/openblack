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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs::archetypes
{
class MistArchetype
{
public:
	/// Map script CREATE_MIST "AFNFF" (handler 0x7155C9 -> Mist::Create 0x6063D0)
	/// @param position x and z of the mist (y is ignored)
	/// @param altitude height above the land (MapCoords relY: the mist is at GetAltitude(x, z) + altitude)
	/// @param colour ARGB, the alpha in the top byte
	/// @param size the mesh scale (LH3DObject +0x88)
	/// @param k the edge-on shrink factor; 1 turns it off (+0x80 bit 2 is only set when k != 1)
	static entt::entity Create(const glm::vec3& position, float altitude, uint32_t colour, float size, float k);
	MistArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
