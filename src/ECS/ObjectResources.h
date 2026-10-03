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

#include "Enums.h"

// Object::GetResource / RemoveResource / AddResource / IsPoisoned of the original (vt +0x98, +0xA0, +0x9C, +0x4A4) for the
// objects that hold resources: the abodes (Abode 0x404D30, 0x404D90, 0x404F10), the storage pit's piles (StoragePitStore), the
// pots and the villagers. Moved unchanged from ecs::abode_villagers (session Edificios, 2026-10-03).

namespace openblack::ecs::object_resources
{
/// Object::GetResource (vt +0x98): Abode 0x404D30 (+0xBC[type]: Abode::foodAmount / woodAmount); a storage pit the
/// total of its piles (PotStructure::GetResource 0x66EF00, StoragePitStore)
[[nodiscard]] uint32_t GetResource(entt::entity object, ResourceType type);
/// Object::RemoveResource (vt +0xA0) for a villager (no GInterfaceStatus): StoragePit 0x7332A0 (StoragePitStore);
/// Abode 0x404F10 -> DoResourceRemoving 0x404F60: amount >= what it has -> SetPoisoned(0) (the abode's own flag:
/// GameThingWithPos has none, nothing); the town's CallDesireFunction(type != 0) BEFORE the removal (the raw desire of
/// Food / Wood rewritten, and maybe HelpSpritesLowOnFood); JustRemoveResource 0x404D60 (min(amount, what it has)).
/// The +0x74 building-site branch (WOOD or -2, V6) is TODO(V6). Returns what was removed
uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount);
/// Abode::AddResource 0x404D90 for a villager (no GInterfaceStatus): DoResourceAdding 0x404DF0 -> JustAddResource
/// 0x404D40 (+0xBC[type] += amount, no cap). Returns the amount
uint32_t AddResource(entt::entity abode, ResourceType type, uint32_t amount);
/// The object's IsPoisoned (vt +0x4A4) as GetResourceFrom 0x7533EC reads it: StoragePit 0x7336B0 =
/// IsPoisonedResource(FOOD) || (WOOD) 0x733550 (an available pile of +0xC4 / +0xC8 whose Pot::IsPoisoned is set);
/// an abode 0 (GameThingWithPos 0x402400); a pot its own flag
[[nodiscard]] bool IsPoisoned(entt::entity object);
} // namespace openblack::ecs::object_resources
