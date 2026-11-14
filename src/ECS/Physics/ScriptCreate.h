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

#include "Enums.h"

namespace openblack::physics
{
class Ground;
} // namespace openblack::physics

namespace openblack::ecs
{
struct PhysicsEntry;
} // namespace openblack::ecs

namespace openblack::ecs::systems
{
class DynamicsSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::ecs::physics::script_create
{
/// What a script's CREATE does to a new mobile object that can become a physics object: it goes into the physics at
/// once with no speed and no spin (no thrower, not from a hand), its body is turned to the slope and lowered until its
/// lowest point touches the land, and it is lifted clear of anything under it. From then on the physics owns where it
/// is, so it settles on the land wherever the script puts it afterwards. Returns its body, or nullptr when it gets none
/// (it cannot become a physics object, a script made it immovable, or it is in the physics already).
ecs::PhysicsEntry* PutIntoPhysics(systems::DynamicsSystemInterface& dynamics, entt::entity object,
                                  const openblack::physics::Ground& ground);

/// What a script's CREATE of a mobile static does to a new thing of the rock class: it goes into the physics at once
/// with no speed and no spin where the script put it, height and pose as they are (not turned to the slope or lowered
/// onto the land), and is lifted clear of anything under it; so it falls from there. Returns its body, or nullptr as
/// PutIntoPhysics.
ecs::PhysicsEntry* PutIntoPhysicsWhereItIs(systems::DynamicsSystemInterface& dynamics, entt::entity object);

/// Whether a mobile object a script's CREATE makes, from the row of the mobile object table it is made from and that
/// row's kind, starts its physics as PutIntoPhysics gives it. The row of a lump of poo always does. A crop's kind makes a
/// field crop, which has no start of physics, and a creed's kind a creed, which can never become a physics object; a
/// mushroom, a magic mushroom or a toadstool only stands on the land. Every other row starts it, the ark's among them.
[[nodiscard]] bool MobileObjectStartsPhysics(MobileObjectInfo row, MobileObjectInfo kind);

/// Whether a script's CREATE of a mobile static or a rock, from the row of the mobile static table it is made from and
/// that row's kind, makes a thing of the rock class, which starts its physics: a row of the rock kind, the bonfire's row
/// aside (it always makes a bonfire). The singing stone base and the lanterns, made apart, are never asked.
[[nodiscard]] bool MakesARock(MobileStaticInfo row, MobileStaticInfo kind);
} // namespace openblack::ecs::physics::script_create
