/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
/// InfluenceRing (Influence.cpp, 0x44 bytes, a GameThingWithPos, not an Object): a circle of a player's influence made
/// by a script or a shield. The logic is in ECS/Influence (influence::CreateRing ...).
struct InfluenceRing
{
	glm::vec3 position;              ///< +0x14 (follows the attached object, ProcessRings 0x5CDB90)
	PlayerNames player;              ///< +0x34
	float radius;                    ///< +0x38
	bool anti;                       ///< +0x3C: an anti-influence ring: no influence of its player inside it
	entt::entity attached {entt::null}; ///< +0x28 BaseInfo: the object it follows (INFLUENCE_OBJECT), or none
};
} // namespace openblack::ecs::components
