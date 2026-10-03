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

#include "ECS/PotResource.h"
#include "Enums.h"

namespace openblack::ecs
{
/// A storage pit as one store (StoragePit / PotStructure in the original): its five wood piles and its food pile hold
/// the store's resources, and the Abode totals (the original's mirror +0xBC[type]) follow them. Spec:
/// dev\documentacion\edificios\V5_resources_spec.md §C.
class StoragePitStore
{
public:
	/// StoragePit::AddResource 0x732F60 (type, n, IS, poisoned, pos, k). Only FOOD and WOOD (0x732FA2 / 0x733083):
	/// food to the food pile (+0xC4), wood to Wood Pile 1..5 (+0xC8..+0xD8) while n != 0, each pile's JustAddResource
	/// (vt +0x8C: the pile sound with the n still asked for, the cap at maxAmountInPot when next < 19, the poison,
	/// SetSize). The pulse: the store held none of it before and something went in -> Town +0x5E8 = 1, +0x5EC = 0
	/// (0x73316D..0x73318D). Then DoResourceAdding 0x404DF0 (0x7331B1; the mirror follows the piles here). Returns the
	/// amount stored (0x7331B9). `dropper` is the GInterfaceStatus (the hand), default none.
	/// (approximate) CREATE_ABODE's initial fill (AbodeArchetype) goes through here too: one pile sound per pile reached
	/// and the pulse; how the original fills a new pit is (pending)
	static uint32_t AddResource(entt::entity store, ResourceType type, uint32_t amount,
	                            const pot_resource::Dropper& dropper = {}, bool poisoned = false);
	/// StoragePit::RemoveResource 0x7332A0 (type, n, IS, out): only FOOD (the food pile) and WOOD (pile 5 -> 1,
	/// 0x7332EF..0x73331B), each pile's JustRemoveResource (object_resources::JustRemoveFromPot: an emptied pile loses its
	/// poison, its reaction and its fire, and stays); something removed -> DoResourceRemoving 0x404F60 (0x733337: the
	/// town's CallDesireFunction with the store's total before the removal). Returns the amount removed.
	static uint32_t RemoveResource(entt::entity store, ResourceType type, uint32_t amount,
	                               const pot_resource::Dropper& dropper = {});
	/// StoragePit::CalulateAmountOverMaximum 0x733260: WOOD total - 5 x GPotInfo[Wood Pile 1].maxAmountInPot ([0xD4CB48]),
	/// any other type total - GPotInfo[Storage Pit Food Pile].maxAmountInPot ([0xD4CA04]); signed
	[[nodiscard]] static int32_t AmountOverMaximum(entt::entity store, ResourceType type);
	/// The Abode totals (the mirror +0xBC) set again from the piles, after a pile changed outside Add / Remove
	static void SyncTotals(entt::entity store);
	/// PotStructure::GetResource 0x66EF00: a store pile reports the store's total.
	[[nodiscard]] static uint32_t GetResource(entt::entity store, ResourceType type);
	/// The storage pit owning a pile, or entt::null.
	[[nodiscard]] static entt::entity OwnerOf(entt::entity pile);
	StoragePitStore() = delete;
};
} // namespace openblack::ecs
