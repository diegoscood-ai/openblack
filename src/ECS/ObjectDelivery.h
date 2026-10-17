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

#include <utility>

#include <entt/entity/entity.hpp>

#include "ECS/PotResource.h"

// A structure (a storage pit, a worship site) takes the resource of an object given to it and the object goes. The
// structure's own handler (ecs::take_resource) calls it between its help trigger and its reaction.
namespace openblack::ecs::object_delivery
{

/// The resource type and amount of an object given to a structure: a tree's wood (ecs::TreeWood), a pot's or a pile's
/// own type and amount, an animal's food, a fence's wood (any other static object has none), any other class's through
/// ecs::object_resources
[[nodiscard]] std::pair<ResourceType, uint32_t> ResourceOf(entt::entity object);

/// `structure` adds the object's resource (its type, amount and poison, by `is`), then with something taken and `is`
/// the local interface PlayResourceDropRemark at the structure, the mulch sound for wood that is not a pot, the object's
/// 500 ms ghost (ecs::object_ghosts) and the object ToBeDeleted. Returns what the structure took
uint32_t DoDeleteObjectAndTakeResource(entt::entity structure, entt::entity object, const pot_resource::Dropper& is);

} // namespace openblack::ecs::object_delivery
