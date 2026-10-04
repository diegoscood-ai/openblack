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

#include <entt/entity/entity.hpp>

#include "ECS/PotResource.h"

// Object::DoDeleteObjectAndTakeResource 0x63A940: a structure (a storage pit, a worship site) takes the resource of an
// object given to it and the object goes. The structure's own DeleteObjectAndTakeResource (session Edificios'
// ecs::take_resource: StoragePit 0x733750, WorshipSite 0x77E7B0) calls it between its help trigger and its reaction.
// Research: dev\_scratch\Edificios\dotr\NOTES.md, dev\documentacion\hand\helpevents\README.md.
namespace openblack::ecs::object_delivery
{

/// 0x63A940 (this = `structure`): AddResource(the object's type, its GetResource, is, its IsPoisoned) (vt +0x9C), then
/// with something taken and `is` the local interface ResourceDropSFX at the structure, the mulch sound for wood that is
/// not a pot, and the object ToBeDeleted. Returns what the structure took
uint32_t DoDeleteObjectAndTakeResource(entt::entity structure, entt::entity object, const pot_resource::Dropper& is);

} // namespace openblack::ecs::object_delivery
