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

#include "ECS/Registry.h"
#include "Locator.h"

// Two bits of GameThingWithPos's u16 flags (+0x24) that the scripts set: SET_ID_MOVEABLE 168 (GScript 0x6FB3E0) and
// SET_ID_PICKUPABLE 169 (0x6FB450) set the bit when their bool is 0 and clear it otherwise. They are also set by
// GameOSFile::LoadInstance 0x559999 and the puzzles (fn_006D71D0, HanoiBlock), not ported. Kept as tag components.
// Wiki: docs/bw1-notes/hand-and-interface.md, "Grabbing".
namespace openblack::ecs::components
{
/// GAME_THING_WITH_POS_FLAG_IMMOVABLE 0x1000
struct Immovable
{
};
/// GAME_THING_WITH_POS_FLAG_CANNOT_BE_PICKED_UP 0x2000
struct CannotBePickedUp
{
};
} // namespace openblack::ecs::components

namespace openblack::ecs::thing_flags
{

/// GameThingWithPos::IsCannotBePickedUp 0x401A10 (vt 0x180, never overridden but by HanoiBlock 0x6DE440, not ported)
[[nodiscard]] inline bool IsCannotBePickedUp(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::CannotBePickedUp>(object);
}

/// SET_ID_PICKUPABLE 169 (0x6FB450): pickupable 0 sets the flag, anything else clears it
inline void SetPickupable(entt::entity object, bool pickupable)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (pickupable)
	{
		registry.Remove<components::CannotBePickedUp>(object);
	}
	else
	{
		registry.AssignOrReplace<components::CannotBePickedUp>(object);
	}
}

/// The flag 0x1000 (PhysicsObject::AddObject 0x6443A0 refuses an immovable object)
[[nodiscard]] inline bool IsImmovable(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::Immovable>(object);
}

/// SET_ID_MOVEABLE 168 (0x6FB3E0): moveable 0 sets the flag, anything else clears it
inline void SetMoveable(entt::entity object, bool moveable)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (moveable)
	{
		registry.Remove<components::Immovable>(object);
	}
	else
	{
		registry.AssignOrReplace<components::Immovable>(object);
	}
}

} // namespace openblack::ecs::thing_flags
