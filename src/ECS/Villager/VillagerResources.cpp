/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerResources.h"

#include <algorithm>

#include <fmt/format.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "Locator.h"

// Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerResources.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// Villager::GetTown (vt +0x48)
Town* TownComponentOf(const Villager& v)
{
	auto& registry = Entities();
	return v.town != entt::null && registry.Valid(v.town) ? registry.TryGet<Town>(v.town) : nullptr;
}

/// +0xE0 bits 14-15: the tree type of the wood carried (PickupResource 0x751460..0x751480)
constexpr uint16_t k_TreeTypeMask = 0xC000;
constexpr uint16_t k_TreeTypeShift = 14;
} // namespace

int16_t PickupResource(entt::entity villager, ResourceType type, int16_t amount, uint8_t treeType)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return amount;
	}
	auto* town = TownComponentOf(*v);
	if (type == ResourceType::Food)
	{
		// 0x751402: add word +0xF4, n; 0x751409..0x751429: with a town, +0x708 += (float)(movsx n)
		v->resourceHeld.at(0) = static_cast<int16_t>(static_cast<uint16_t>(v->resourceHeld.at(0)) + static_cast<uint16_t>(amount));
		if (town != nullptr)
		{
			town->stats.foodCarried = town->stats.foodCarried + static_cast<float>(amount);
		}
		return amount;
	}
	// 0x751435: add word +0xF6, n; 0x75143E..0x75145E: with a town, +0x70C += n; 0x751460..0x751480: the tree type
	v->resourceHeld.at(1) = static_cast<int16_t>(static_cast<uint16_t>(v->resourceHeld.at(1)) + static_cast<uint16_t>(amount));
	if (town != nullptr)
	{
		town->stats.woodCarried = town->stats.woodCarried + static_cast<float>(amount);
	}
	v->flags = static_cast<uint16_t>((v->flags & ~k_TreeTypeMask) | ((static_cast<uint16_t>(treeType) & 3u) << k_TreeTypeShift));
	return amount;
}

void PickupFood(entt::entity villager, int16_t amount)
{
	// 0x751490: PickupResource(FOOD, n, 0)
	PickupResource(villager, ResourceType::Food, amount, 0);
}

uint16_t DropFood(entt::entity villager, uint16_t amount)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x7511E6..0x751202: n == 0, or n above +0xF4 (cmp di, ax; jbe: unsigned 16 bits) -> n = +0xF4
	const auto held = static_cast<uint16_t>(v->resourceHeld.at(0));
	if (amount == 0 || amount > held)
	{
		amount = held;
	}
	// 0x751204..0x751206: +0xF4 -= n; 0x75120D..0x751236: with a town, +0x708 -= (float)(n & 0xFFFF)
	v->resourceHeld.at(0) = static_cast<int16_t>(static_cast<uint16_t>(held - amount));
	if (auto* town = TownComponentOf(*v))
	{
		town->stats.foodCarried = town->stats.foodCarried - static_cast<float>(amount);
	}
	return amount;
}

uint16_t GetResourceFrom(entt::entity villager, entt::entity object, ResourceType type, int16_t amount)
{
	// 0x7533A0..0x7533AD: object.RemoveResource(type, (movsx) n, 0, 0) (vt +0xA0). A negative n is a huge unsigned:
	// the whole store (literal)
	const auto requested = static_cast<uint32_t>(static_cast<int32_t>(amount));
	const auto taken = static_cast<uint16_t>(object_resources::RemoveResource(object, type, requested));
	// 0x7533B5: test bx, bx
	if (taken == 0)
	{
		return 0;
	}
	// 0x7533BA..0x7533C9: PickupResource(type, c, object.GetCarriedTreeType()): Object 0x402AF0 is `xor eax, eax`
	PickupResource(villager, type, static_cast<int16_t>(taken), 0);
	// 0x7533CE..0x7533E2: object.IsSpeedUp (vt +0x4A8): GameThingWithPos 0x402410 `xor al, al` for abodes and storage
	// pits (the piles' own is V5): no SetFoodSpeedup here
	// 0x7533E8..0x7533FC: object.IsPoisoned (vt +0x4A4) -> SetPoisoned(1)
	if (object_resources::IsPoisoned(object))
	{
		life::TakePoisonedResource(villager);
	}
	return taken;
}

entt::entity GetStoragePit(entt::entity villager)
{
	// 0x751F10: GetTown() && Town::GetStoragePit -> it; else GetAbode()
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return entt::null;
	}
	if (v->town != entt::null && Entities().Valid(v->town))
	{
		if (const auto pit = tq::GetStoragePit(v->town); pit != entt::null)
		{
			return pit;
		}
	}
	return v->abode != entt::null && Entities().Valid(v->abode) ? v->abode : entt::null;
}

glm::ivec2 GetResourceDropoffPos(entt::entity villager, [[maybe_unused]] ResourceType type)
{
	const auto* v = VillagerOf(villager);
	const auto me = tq::PosOf(villager);
	if (v == nullptr)
	{
		return me;
	}
	// 0x753E49..0x753E79: GetStoragePit functional (vt +0xD4) -> its GetArrivePos (vt +0x104)
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		return abode_queries::GetArrivePos(pit);
	}
	// 0x753E7C..0x753E99: with a town, the town's storage pit functional -> its GetArrivePos
	const auto town = v->town != entt::null && Entities().Valid(v->town) ? v->town : entt::null;
	if (town != entt::null)
	{
		if (const auto pit = tq::GetStoragePit(town); pit != entt::null && abode_queries::IsFunctional(pit))
		{
			return abode_queries::GetArrivePos(pit);
		}
		// 0x753E9B..0x753ECC: Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900: the temporary pot near
		// the congregation point (MagicFood 10, Town +0x600 / +0x604). TODO(V5): not ported; (aproximado hasta V5) the
		// villager's own position, so ChangeStateToFindFoodToEat sees "already there" and goes on to what it carries
		return me;
	}
	// 0x753ED3..0x753EEB: no town -> my position
	return me;
}

glm::ivec2 GetResourceNearestEdge(entt::entity object, [[maybe_unused]] ResourceType type, [[maybe_unused]] entt::entity villager)
{
	// vt +0x8D4: StoragePit::GetResourceNearestEdge 0x733400 -> GetArrivePos (vt +0x104);
	// MultiMapFixed::GetResourceNearestEdge 0x401590 -> GetResourcePos (vt +0x8CC, 0x401560): the object's +0x14
	if (Entities().AllOf<StoragePit>(object))
	{
		return abode_queries::GetArrivePos(object);
	}
	return tq::PosOf(object);
}

uint32_t AtStructureRemoveResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t amount)
{
	// 0x76A31C: pos = object.GetResourceNearestEdge(type, this, 1)
	const auto pos = GetResourceNearestEdge(object, type, villager);
	// 0x76A33A..0x76A351: MapCoords::IsCloseToEqual(pos, GetRadius (vt +0x60 = Get2DRadius)) 0x6053C0: distance <= r
	const auto me = tq::PosOf(villager);
	if (tq::GetDistanceInMetres(me, pos) <= tq::Get2DRadius(villager))
	{
		// 0x76A353..0x76A381: c = GetResourceFrom(object, type, n); 0 -> 0; c < n (cmp, sbb) -> 0x24; else 1
		const auto c = static_cast<uint32_t>(GetResourceFrom(villager, object, type, static_cast<int16_t>(amount)));
		if (c == 0)
		{
			return 0;
		}
		return c < amount ? 0x24u : 1u;
	}
	// 0x76A384..0x76A39E: SetupMoveToWithHug(pos, GetFinalState); 0x24
	SetupMoveToWithHug(villager, tq::ToMetres(pos), GetFinalState(villager));
	return 0x24;
}

uint32_t ArrivesAtStoragePitForResource(entt::entity villager, ResourceType type, uint32_t amount, VillagerStates ok,
                                        VillagerStates fail)
{
	const auto* v = VillagerOf(villager);
	// 0x7698DC..0x7698E1: n <= 0 (unsigned: 0) -> SetTopState(fail); 1
	if (amount == 0 || v == nullptr)
	{
		SetTopState(villager, fail);
		return 1;
	}
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		// 0x76990D..0x769939: m = min(n, pit.GetResource(type)) (cmp; jb keeps n)
		const uint32_t have = object_resources::GetResource(pit, type);
		const uint32_t m = amount < have ? amount : have;
		// 0x76993B: m == 0 -> SetTopState(fail); 0
		if (m == 0)
		{
			SetTopState(villager, fail);
			return 0;
		}
		// 0x76993F..0x76994C: AtStructureRemoveResource(pit, type, m, 0)
		const auto r = AtStructureRemoveResource(villager, pit, type, m);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("food 34: pit {} has {} need {} -> {:#x}", static_cast<uint32_t>(pit), have, amount, r));
		}
		// 0x769951: 0x24 -> 0x24 (on the way, or part of it)
		if (r == 0x24)
		{
			return 0x24;
		}
		// 0x769960..0x769994: 1 and ok != 0 -> SetupMoveToOnFootpath(pit, pit.GetArrivePos, ok); 1
		if (r == 1 && ok != VillagerStates::InvalidState)
		{
			SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), ok);
			return 1;
		}
		// 0x7699A3: SetTopState(fail); 0
		SetTopState(villager, fail);
		return 0;
	}
	// 0x7699BE..0x769B07: no functional pit: with a town, GetTemporaryResourceStorePotOrPos 0x73E900's pot (its nearest
	// edge, AreWeThere, RemoveResource, PickupResource; ok -> SetTopState(ok)). TODO(V5): no temporary pots, so the pot
	// is null -> 0x769B0A
	// 0x769B0A: SetTopState(fail); 1 (also without a town)
	if (TraceOn(villager))
	{
		Trace(villager, "food 34: no functional storage pit (TODO V5: the temporary pot) -> 163");
	}
	SetTopState(villager, fail);
	return 1;
}
} // namespace openblack::ecs::villager
