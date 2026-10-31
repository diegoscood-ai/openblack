/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include <entt/entity/entity.hpp>

#include "ECS/Registry.h"
#include "Locator.h"

// Two bits of an object's flags that the scripts set: SET_ID_MOVEABLE 168 and SET_ID_PICKUPABLE 169 set the bit when
// their bool is 0 and clear it otherwise. They are also set by the saved game's load and the puzzles (the Hanoi
// blocks), not ported. A third bit is the hand's: an object in its locked select. Kept as tag components.
// Wiki: docs/wiki/hand/grab-hold-and-throw.md#grabbing, docs/wiki/hand/states.md#the-locked-select.
namespace openblack::ecs::components
{
/// The object cannot be moved (the immovable flag)
struct Immovable
{
};
/// The object cannot be picked up
struct CannotBePickedUp
{
};
/// The object is in a hand's locked select: set when the select's start packet takes it, whatever the object's own
/// start then gives, and cleared only when its end packet is applied. The scripts read it (IS_LOCKED_INTERACTION).
/// Wiki: docs/wiki/hand/states.md#what-the-scripts-read-of-it
struct InLockedSelect
{
};
} // namespace openblack::ecs::components

namespace openblack::ecs::object_flags
{

/// Whether the object cannot be picked up (in the original only the Hanoi blocks, not ported, answer otherwise)
[[nodiscard]] inline bool IsCannotBePickedUp(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::CannotBePickedUp>(object);
}

/// SET_ID_PICKUPABLE 169: pickupable 0 sets the flag, anything else clears it
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

/// The immovable flag (an object in physics or immovable gets no physics)
[[nodiscard]] inline bool IsImmovable(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<components::Immovable>(object);
}

/// SET_ID_MOVEABLE 168: moveable 0 sets the flag, anything else clears it
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

/// Whether the object is in a hand's locked select, from the start packet that took it until its end packet is applied
[[nodiscard]] inline bool IsInLockedSelect(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return registry.Valid(object) && registry.AllOf<components::InLockedSelect>(object);
}

/// Sets or clears the object's locked-select flag (a hand's start and end packets). Nothing for an object that is gone
inline void SetInLockedSelect(entt::entity object, bool inLockedSelect)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (inLockedSelect)
	{
		registry.AssignOrReplaceState<components::InLockedSelect>(object);
	}
	else if (std::as_const(registry).AllOf<components::InLockedSelect>(object))
	{
		registry.RemoveState<components::InLockedSelect>(object);
	}
}

} // namespace openblack::ecs::object_flags
