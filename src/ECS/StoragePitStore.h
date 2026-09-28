/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs
{
/// A storage pit as one store (StoragePit / PotStructure in the original): its five wood piles and its food pile hold
/// the store's resources, and the Abode totals follow them.
class StoragePitStore
{
public:
	/// StoragePit::AddResource 0x732F60: wood fills Wood Pile 1..5 in order (each clipped at maxAmountInPot except the
	/// last, whose nextPotForResource is none); food goes to the single food pile. Returns the amount stored.
	static uint32_t AddResource(entt::entity store, ResourceType type, uint32_t amount);
	/// StoragePit::RemoveResource 0x7332A0: wood is taken from the last pile first (5 -> 1), whichever pile the hand is
	/// over. Returns the amount removed.
	static uint32_t RemoveResource(entt::entity store, ResourceType type, uint32_t amount);
	/// PotStructure::GetResource 0x66EF00: a store pile reports the store's total.
	[[nodiscard]] static uint32_t GetResource(entt::entity store, ResourceType type);
	/// The storage pit owning a pile, or entt::null.
	[[nodiscard]] static entt::entity OwnerOf(entt::entity pile);
	StoragePitStore() = delete;
};
} // namespace openblack::ecs
