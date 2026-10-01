/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The held object applied to the object under the hand (GInterface::ActionPressedHolding 0x5D1560 for an object that is
// not a spell seed, SendApplyToObject 0x5D30D0, the packet 0x11 handler 0x5DA1A0 and HandleApplyResult fn_005DA100),
// the class dispatch of ValidToApplyThisToObject (vt 0x71C) / ApplyThisToObject (vt 0x720) for the held classes that
// are ported (Villager 0x752BD0 / 0x752C40: a villager dropped into a teleport stone), and the pick-up of a spell seed
// or a teleport stone (GenericPickup 0x5D2800 -> PlaceObjectInMagicHand 0x5DA6F0). Wiki: docs/bw1-notes/miracles.md,
// "Teletransporte" and "Bosque".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/InterfaceStatus.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
namespace teleport = magic::teleport;

/// ApplyThisTo* results (HandleApplyResult fn_005DA100)
constexpr int k_ResultConsumed = 3;
constexpr int k_ResultNothing = 5;
constexpr int k_ResultRemoved = 0x16;
constexpr int k_ResultPlaced = 0x17;
constexpr int k_ResultRemovedOnly = 0x18;

bool ApplyTrace()
{
	static const bool trace = std::getenv("OPENBLACK_HAND_TRACE") != nullptr || std::getenv("OPENBLACK_TELEPORT_TRACE") != nullptr ||
	                          std::getenv("OPENBLACK_SPELL_TRACE") != nullptr;
	return trace;
}

/// Villager::ValidToApplyThisToObject 0x752BD0: a WorshipTotem (RTDynamicCast 0x752BE7) -> 1, a MagicTeleport (0x752C0C)
/// -> ValidToApplyVillagerDirectlyToTeleport fn_005FC4B0 == 1, else 0.
/// (pendiente) TODO(worship): the WorshipTotem branch (the sacrifice of ApplyThisToObject 0x752C40 up to 0x752FB0) is
/// not ported: here a totem does not take the villager, and the press arms the put down as before
bool VillagerValidToApplyThisToObject(entt::entity villager, entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MagicTeleport>(target))
	{
		return teleport::ValidToApplyVillagerDirectly(target, villager);
	}
	return false;
}

/// vt 0x71C ValidToApplyThisToObject of the held object. Object 0x4028B0 returns 0; of the other held classes only the
/// Villager is ported here (SpellSeed 0x7286D0 is HandSpellSeed.cpp's)
bool ValidToApplyThisToObject(entt::entity held, entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(held))
	{
		return VillagerValidToApplyThisToObject(held, target);
	}
	return false;
}
} // namespace

entt::entity HandSystem::SeedToPlaceInHand(entt::entity object) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return entt::null;
	}
	if (registry.AllOf<SpellSeed>(object))
	{
		// SpellSeed::ValidForPlaceInHand 0x728580 with the local hand's player
		return magic::seed::ValidForPlaceInHand(object, PlayerNames::PLAYER_ONE) ? object : entt::null;
	}
	if (registry.AllOf<MagicTeleport>(object))
	{
		// MagicTeleport::ValidForPlaceInHand 0x5FC440: spell +0x9C, its seed +0xAC, then the seed's vt 0x6FC; 0 without
		const auto seed = teleport::SeedOf(object);
		return seed != entt::null && magic::seed::ValidForPlaceInHand(seed, PlayerNames::PLAYER_ONE) ? seed : entt::null;
	}
	return entt::null;
}

bool HandSystem::PickUpSeedOrStone(entt::entity object, bool inInfluence) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AnyOf<SpellSeed, MagicTeleport>(object))
	{
		return false;
	}
	// GenericPickup 0x5D2800: ValidForPlaceInHand (vt 0x6FC) == 1, and in the influence: both keep Object's
	// InterfaceMustBeInInfluenceForInteraction 0x4028A0 = 1 (vt 0x714)
	const auto seed = SeedToPlaceInHand(object);
	const bool stone = registry.AllOf<MagicTeleport>(object);
	if (seed == entt::null || !inInfluence)
	{
		return true;
	}
	// 0x5D2881..0x5D28B7 (after packet 0x13): neither a tree nor a forest, so SoundTag::Create(the object's MapCoords
	// +0x14, 10 G_PickUpObject, track 0, mode 3, loops 0, +0x40 0, is3D 1, InGame, delay 0) 0x71EB60, as HandHolding.cpp
	// PickUp does. The point is taken before the stone goes. (aproximado) a seed's MapCoords altitude is not moved by its
	// draw 0x729020 (only the Game3DObject's matrix); openblack keeps only the drawn point in its Transform: that one
	if (const auto* transform = registry.TryGet<const Transform>(object); transform != nullptr)
	{
		const auto at = stone ? teleport::MapPositionOf(object) : magic::ToMap(transform->position);
		audio::tags::CreateAtMapCoords(at.x, at.z, at.y, 10, false, 3, 0, false, true, audio::SfxBank::InGame, 0);
	}
	// packet 0x13 -> GInterface::PlaceObjectInMagicHand 0x5DA6F0 (the object's vt 0x700): a SpellSeed's
	// InterfaceSetInMagicHand 0x728810 (StoreChantsAndAgeFromSpell 0x728780, ClearSpellLink 0x728200: its spell closes
	// down) and the hand takes it; a MagicTeleport's 0x5FC470 is GInterfaceStatus::PlaceObjectInMagicHand(its seed)
	// 0x5DC870 -> 0x5DA6F0 on the seed (the same), and returns 0, so the outer call only ends the action and the hand
	// shows its first object, the seed. The seed's spell closes (SpellWithObjects::CloseDown 0x721300) and the stone goes.
	const int result = worship::interface::PlaceSeedInMagicHand(PlayerNames::PLAYER_ONE, seed);
	_pickPressHeld = _held.has_value();
	if (ApplyTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: picked up {} {} -> seed {} (InterfaceSetInMagicHand {}), held {}",
		                   stone ? "teleport stone" : "spell seed",
		                   static_cast<uint32_t>(object), static_cast<uint32_t>(seed), result,
		                   _held ? static_cast<int>(*_held) : -1);
	}
	return true;
}

std::optional<entt::entity> HandSystem::RemoveFirstFromHand() noexcept
{
	// fn_005CED60: GInterfaceStatus fn_005DC1E0 pops the GMagicHand (RemoveFromHand 0x5FB0B0: FireEffect::SetOutMagicHand),
	// and the local hand empties (CHand::ThrowObject 0x46DDD0: no physics)
	if (!_held)
	{
		return std::nullopt;
	}
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	_releaseArmed = false;
	if (Locator::entitiesRegistry::value().Valid(entity))
	{
		ecs::fire::SetOutMagicHand(entity);
	}
	return entity;
}

bool HandSystem::HeldActionPressedOnObject(bool inInfluence) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!_held || !registry.Valid(*_held))
	{
		return false;
	}
	const auto held = *_held;
	// GetCreatureToGiveTo (no creature), else m_ActionCollide.object [this+0x400]: the object under the hand
	const auto target = _cursorObject && registry.Valid(*_cursorObject) ? *_cursorObject : entt::null;
	// ValidAsInterfaceTarget vt 0x6F0 (Object 0x402840 = 1). InterfaceValidToGiveObject vt 0x748 needs a creature (M8).
	// 0x5D15D6..0x5D15E9: out of the influence ([this+0x48] == 0) and InterfaceMustBeInInfluenceForInteraction (vt 0x714,
	// Object 0x4028A0 = 1) -> 0x5D16BE, the branch without a target
	if (target == entt::null || target == held || !inInfluence)
	{
		return false;
	}
	// 0x5D1607: held->ValidToApplyThisToObject(status, target) == 1; else 0x5D168E (not a seed: ApplyOnlyAfterReleased /
	// FailApply, the put down that the press arms here)
	if (!ValidToApplyThisToObject(held, target))
	{
		return false;
	}
	// a villager: ApplyOnlyAfterRecSystem vt 0x738 (Object 0x402920) = 0 and ValidForLockedApplyProcess vt 0x72C (Object
	// 0x4028F0) = 0, so SendApplyToObject 0x5D30D0 (0x5D1684): the hand holds something (status +0x90, 0x5D30FA),
	// ValidAsInterfaceTarget, the validity again (0x5D3126), not a seed: packet 0x11 (0x5D32A7) and action state 0x12
	// (0x5D32B9); the packet's 0x5DA1A0 checks the same (IsInteractable, +0x90, vt 0x71C) and calls vt 0x720 (0x5DA251)
	int result = 0;
	if (registry.AllOf<Villager>(held) && registry.AllOf<MagicTeleport>(target))
	{
		// Villager::ApplyThisToObject 0x752C40: a MagicTeleport (0x752FC2) whose ValidToApplyVillagerDirectlyToTeleport
		// (0x752FEA) == 1 -> fn_005FC4F0 (0x752FF8): FLYING, fn_005DA0C0 (RemoveFirstFromHand and the villager put at the
		// stone's MapCoords), LANDED, DecideWhatToDo, its destination registered and DoTeleport(forced). 1 when it jumped,
		// else 0x17. Not valid any more -> 0 (0x75300D).
		if (teleport::ValidToApplyVillagerDirectly(target, held))
		{
			RemoveFirstFromHand(); // fn_005DA0C0's part; ApplyVillagerDirectly puts it at the stone (LandAt)
			result = teleport::ApplyVillagerDirectly(target, held);
		}
	}
	// HelpProfile::Trigger(7 for a sacrifice altar, else 6) for the local interface: (pendiente) TODO(M2) help
	// HandleApplyResult fn_005DA100 (result, held, &target->Pos)
	const bool stillHeld = _held && *_held == held;
	if (result == k_ResultNothing)
	{
		result = 0;
	}
	else if (result != k_ResultConsumed && (result == 1 || stillHeld))
	{
		// fn_006E47C0(held, 1): +0x30 = 1 on held's entry of the list g_game +0x205BDC (not identified, not ported)
		if (result == k_ResultRemoved)
		{
			RemoveFirstFromHand();
		}
		else if (result == k_ResultPlaced && stillHeld)
		{
			// fn_005DA0C0(target pos, held): out of the hand, put at the target's MapCoords and InsertMapObject
			// (inferido: openblack has no other class that returns 0x17 with the object still in the hand)
			RemoveFirstFromHand();
			if (auto* transform = registry.TryGet<Transform>(held); transform != nullptr)
			{
				transform->position = registry.Get<const Transform>(target).position;
				registry.SetDirty();
			}
		}
		else if (result == k_ResultRemovedOnly)
		{
			RemoveFirstFromHand(); // GInterfaceStatus fn_005DC1E0
		}
	}
	_releaseArmed = false;
	if (ApplyTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: applied {} to object {}: result {:#x}, still held {}",
		                   static_cast<uint32_t>(held), static_cast<uint32_t>(target), result, _held.has_value());
	}
	return true;
}
