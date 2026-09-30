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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// MagicTeleport (a MobileStatic, so a MultiMapFixed: 0xA4 bytes, vtable 0x92C110; ctor 0x5FC130): the invisible
/// teleport stone each TELEPORT cast leaves. No mesh; its vortex PSys (SF_TeleportVortex) is its only visual.
/// Magic/Objects/MagicTeleport.
struct MagicTeleport
{
	/// +0x8C / +0x90: {Living*, the MapCoords it wants to reach} of each registered traveller (newest first)
	struct Traveller
	{
		entt::entity living {entt::null};
		glm::vec3 destination {0.0f}; ///< MapCoords as metres (y above the land)
	};
	std::vector<Traveller> travellers;
	uint32_t reaction {0};           ///< +0x94 REACTION 20 REACT_TO_TELEPORT (ECS/Effects/Reactions.h id)
	uint32_t psys {0};               ///< +0x98 the SF_TeleportVortex effect (psys::manager id), 0 none
	entt::entity spell {entt::null}; ///< +0x9C the SpellTeleport
	bool hasPlayer {false};          ///< +0xA0 != NULL
	PlayerNames player {PlayerNames::NEUTRAL}; ///< +0xA0 the spell's player (GetPlayer 0x5FC430)
	float scale {1.0f};              ///< GameThingWithPos +0x? scale: GetScale() x 0.01 at Create
};

} // namespace openblack::ecs::components
