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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::archetypes
{
/// A town centre's totem: the tribe's plinth at the centre's totem point, with the centre's turn and scale, and an icon
/// standing on it
class VillageTotemArchetype
{
public:
	/// The town centre's totem (a built town centre of a town, when made functional): the tribe's plinth (the totem
	/// statue info by the tribe) at the town centre's special point 6 (through its full matrix; raised like the morphing
	/// town centre, by the land under the point minus the land under the origin), with its Y angle and scale, and the
	/// icon on the plinth's top. Both static, no foundation sink (not an Abode). It is then given to its town's player
	/// (VillageTotemSystemInterface::AddToPlayer): it faces the town's worship site if the town has one already, and
	/// wears the icon of the player's creature.
	static void Create(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale);
	/// The totem's own parts, of the town centre given (null: none): the tribe's plinth at the point, with the Y angle and
	/// scale, and the icon on the plinth's top, its town centre not having it yet (VillageTotem::onCentre false). Given to
	/// no player. None for a tribe with no totem statue row
	static entt::entity CreateAt(entt::entity townCentre, size_t tribe, const glm::vec3& point, float yAngleRadians,
	                             float scale);
	VillageTotemArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
