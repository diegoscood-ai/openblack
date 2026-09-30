/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A town centre's totem statue (TotemStatue, TotemStatue::Create 0x737CC0), on the entity of its plinth (the tribe's
/// BuildingPlayerIconPlinth mesh, GTotemStatueInfo). `top` is the entity of the icon standing on it: the player's
/// creature (BuildingPlayerIcon<Species>) or, without a creature, the hand (BuildingSpellHand). Both rise out of the
/// ground with the town's worship (Draw 0x738960: 8 x the worship fraction; openblack has no worship yet: 0).
struct TotemStatue
{
	/// the plinth's top above its origin (0x999A9C): where the icon stands
	static constexpr float k_PlinthTop = 2.729f;
	/// how far the statue rises at full worship
	static constexpr float k_WorshipRise = 8.0f;

	entt::entity townCentre {entt::null};
	entt::entity top {entt::null};
	float baseY {0.0f}; ///< the plinth's y with no worship
	float rise {0.0f};  ///< the current rise, 0 .. k_WorshipRise
};

} // namespace openblack::ecs::components
