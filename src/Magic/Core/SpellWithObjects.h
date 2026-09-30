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

#include <entt/entity/entity.hpp>

// SpellWithObjects (Spell + a list of the objects it made, +0xEC head / +0xF0 count): food and wood piles, magic trees,
// flock animals. The spell lives while its PSys or any of its objects does.

namespace openblack::ecs::components
{
struct SpellObjects
{
	std::vector<entt::entity> objects; ///< newest first, as the {next, GameThing*} list
};
} // namespace openblack::ecs::components

namespace openblack::magic::spell_objects
{
/// the list's push-front
void Add(entt::entity spell, entt::entity object);
void Remove(entt::entity spell, entt::entity object);
[[nodiscard]] const std::vector<entt::entity>& Objects(entt::entity spell);

/// ProcessSpellObjectsAndRemoveToBeDeleted 0x721040 (the objects that are gone leave the list): true if any is left.
/// TODO(M3/M4): the per-object processing of 0x721040 comes with the classes that make objects.
bool ProcessObjectsAndRemoveDeleted(entt::entity spell);

/// SpellWithObjects::Process 0x721290: CoreProcess; 5 only when no object is left and the PSys is gone
int Process(entt::entity spell);
} // namespace openblack::magic::spell_objects
