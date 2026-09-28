/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class AbodeArchetype
{
public:
	static entt::entity Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians, float scale,
	                           uint32_t foodAmount, uint32_t woodAmount);
	/// StoragePit::AddResource 0x732F60: wood fills Wood Pile 1..5 in order (each clipped at maxAmountInPot except the
	/// last, whose nextPotForResource is none); food goes to the single food pile. Returns the amount stored.
	static uint32_t AddToStoragePit(entt::entity store, ResourceType type, uint32_t amount);
	/// StoragePit::RemoveResource 0x7332A0: wood is taken from the last pile first (5 -> 1), whichever pile the
	/// hand is over. Returns the amount removed.
	static uint32_t RemoveFromStoragePit(entt::entity store, ResourceType type, uint32_t amount);
	/// PotStructure::GetResource 0x66EF00: a store pile reports the store's total.
	[[nodiscard]] static uint32_t StoragePitAmount(entt::entity store, ResourceType type);
	/// The storage pit owning a pile, or entt::null.
	[[nodiscard]] static entt::entity StoragePitOfPile(entt::entity pile);
	AbodeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
