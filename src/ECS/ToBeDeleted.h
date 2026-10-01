/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// GameThing::ToBeDeleted (vt +0xC) per class, the common "this object goes" (the physics' -4R deletion 0x645B22, a sunk
/// animal, a drowned villager...): the class's own clean-up, then out of the physics and out of the registry.
///
/// - Villager::ToBeDeleted 0x7521B0 -> DeleteDependancys 0x74FD60: openblack's part is leaving its home and its town's
///   homeless list;
/// - Animal::ToBeDeleted 0x417B60 -> DeleteDependancys 0x417BA0: the animal AI forgets it (flock, prey, hunter);
/// - Tree::ToBeDeleted 0x74A210: out of its forest (fn_0053A220) and of the game's tree list [g_game+0x205CDC]; in
///   openblack a tree only carries its forest id, so nothing is left to unlink;
/// - Object::ToBeDeleted 0x636670: the rest (its shadow / attached thing at +0x44).
/// The registry entity is destroyed at once (the original marks it and deletes it later in the turn).
void ToBeDeleted(entt::entity entity);

} // namespace openblack::ecs
