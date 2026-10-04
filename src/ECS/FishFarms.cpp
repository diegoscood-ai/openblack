/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishFarms.h"

#include <algorithm>
#include <cmath>

#include "Common/GameRandom.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Fields.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

// FishFarm.cpp of runblack.exe W120 (FishFarms.h)

namespace openblack::ecs::fish_farms
{
using namespace components;

namespace
{
FishFarm* FarmOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<FishFarm>(entity) : nullptr;
}

/// entt's on_destroy<FishFarm> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): FishFarm::ToBeDeleted
/// 0x52C690 -> DeleteDependancys 0x52C693. The component is still there during the signal
void OnFishFarmDestroyed(entt::registry& /*registry*/, entt::entity entity)
{
	DeleteDependancys(entity);
}

/// GFishFarmInfo (+0x28): info.dat has only row 0, InfoConstants::fishFarm
const GFishFarmInfo& InfoOf()
{
	return Locator::infoConstants::value().fishFarm;
}
} // namespace

void AddFisherman(entt::entity farm, entt::entity villager)
{
	auto* f = FarmOf(farm);
	// 0x52D256..0x52D25A: a null villager -> nothing (the write at 0x52D281 would be through NULL)
	if (f == nullptr || villager == entt::null)
	{
		return;
	}
	// 0x52D25C..0x52D27B: a new node at the head, +0x84++. The deletion listener connected first (entt's sink::connect
	// is idempotent: once per registry)
	Locator::entitiesRegistry::value().OnDestroy<FishFarm>().connect<&OnFishFarmDestroyed>();
	f->fishermen.insert(f->fishermen.begin(), villager);
	// 0x52D281: villager +0x118 = this
	// TODO(Personas): villager::SetTargetThing(villager, farm); // fn_0052D250 0x52D281
}

void RemoveFisherman(entt::entity farm, entt::entity villager)
{
	if (auto* f = FarmOf(farm); f != nullptr)
	{
		// 0x52D2A6..0x52D2DD: every node of the villager unlinked and freed, +0x84-- each; +0x118 untouched
		std::erase(f->fishermen, villager);
	}
}

bool HasFisherman(entt::entity farm, entt::entity villager)
{
	const auto* f = FarmOf(farm);
	return f != nullptr && std::find(f->fishermen.begin(), f->fishermen.end(), villager) != f->fishermen.end();
}

uint32_t FishermanCount(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	return f != nullptr ? static_cast<uint32_t>(f->fishermen.size()) : 0u;
}

int32_t Score(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	if (f == nullptr)
	{
		return 0;
	}
	// 0x52D2F3..0x52D318: fild qword {+0x84, 0}; fidiv dword maxNoFishermanPerFishFarm (+0x120, read signed)
	const auto maximum = static_cast<float>(static_cast<int32_t>(InfoOf().maxNoFishermanPerFishFarm));
	// (the 24-bit FPU, fn_007DEE00: the fidiv, fsub and fmul are each rounded to float)
	const float ratio = static_cast<float>(f->fishermen.size()) / maximum;
	// 0x52D31C..0x52D331: fcom 1; test ah, 1; je: not below 1 (and ordered) -> 1.0; else the ratio (fstp dword)
	const float share = ratio < 1.0f || std::isnan(ratio) ? ratio : 1.0f;
	// 0x52D339..0x52D34A: fn_0052D240 (fld 1.0) x (1 - share); __ftol 0x7A1400 (FtoL: its SSE2 branch)
	constexpr float k_FishFarmWeight = 1.0f; // fn_0052D240 0x52D240: fld [0x8AA390]
	const float rest = 1.0f - share;
	return map_coords::FtoL(k_FishFarmWeight * rest);
}

map_coords::MapCoords FishingSpot(entt::entity farm)
{
	auto pos = object::MapCoordsOf(farm);
	// 0x52C88A..0x52C8A6: r = Get2DRadius (vt +0x64: 5.0, 0x52C470); h = r x 0.5
	const float r = object::Get2DRadius(farm);
	const float h = r * 0.5f;
	// 0x52C8AA ("FishFarm.cpp" 0xB8) for x, then 0x52C8C2 (0xB9) for z: GameFloatRand(r) - h
	const float a = game_random::GameFloatRand(r) - h;
	const float b = game_random::GameFloatRand(r) - h;
	// 0x52C8CB..0x52C926: ftol((pos x 10 x 2^-16 + d) x 65536 / 10) on each axis; the altitude (+8) copied
	pos.x = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.x) + a);
	pos.z = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.z) + b);
	return pos;
}

map_coords::MapCoords GetArrivePos(entt::entity farm)
{
	// 0x52C490: the position +0x14
	return object::MapCoordsOf(farm);
}

int32_t RemoveFood(entt::entity farm, int32_t amount)
{
	auto* f = FarmOf(farm);
	if (f == nullptr)
	{
		return 0;
	}
	// 0x52CED0..0x52CEE2: fild n; fcom food; test ah, 0x41; je: n above food -> what is left
	const auto n = static_cast<double>(amount);
	if (!(n > static_cast<double>(f->food)))
	{
		// 0x52CEE4..0x52CEF0: fld food; fsub st(1); fstp food; EAX = n
		f->food = static_cast<float>(static_cast<double>(f->food) - n);
		return amount;
	}
	// 0x52CEFC..0x52CF09: ftol(food); food = 0
	const int32_t left = map_coords::FtoL(f->food);
	f->food = 0.0f;
	return left;
}

int32_t RemoveResource(entt::entity farm, ResourceType type, int32_t amount)
{
	// 0x52CF20..0x52CF37: type 0 (FOOD) -> RemoveFood(n); else 0
	return type == ResourceType::Food ? RemoveFood(farm, amount) : 0;
}

entt::entity TownOf(entt::entity farm)
{
	const auto* f = FarmOf(farm);
	auto& registry = Locator::entitiesRegistry::value();
	// 0x52C450: +0x8C
	return f != nullptr && f->town != entt::null && registry.Valid(f->town) ? f->town : entt::null;
}

std::vector<entt::entity> TownFishFarms(entt::entity town)
{
	std::vector<entt::entity> list;
	if (town == entt::null)
	{
		return list;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
		if (farm.town == town)
		{
			list.push_back(entity);
		}
	});
	// the ctor's head insertion (0x52C407..0x52C418): newest first
	std::sort(list.begin(), list.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

void DisconnectDeletionListener()
{
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().OnDestroy<FishFarm>().disconnect<&OnFishFarmDestroyed>();
	}
}

bool IsFishFarm(entt::entity thing)
{
	return FarmOf(thing) != nullptr;
}

void DeleteDependancys(entt::entity farm)
{
	// 0x52C6B3..0x52C6D7: while +0x80: the head villager's SetTopState(163) (vt +0x8E8); ExitFishing 0x75B880 unlinks
	// it (RemoveFisherman), the farm still being available (GameThing +0xA bit 1 is set later, 0x56FB7B)
	for (auto* f = FarmOf(farm); f != nullptr && !f->fishermen.empty(); f = FarmOf(farm))
	{
		const auto before = f->fishermen.size();
		fields::ReleaseWorker(f->fishermen.front());
		f = FarmOf(farm);
		// (openblack guard) nothing unlinked it: stop instead of looping for ever
		if (f == nullptr || f->fishermen.size() >= before)
		{
			break;
		}
	}
	// 0x52C6DA..0x52C741 (out of the town's +0x788), 0x52C75E..0x52C7A7 (out of g_game +0x205C0C): nothing to do (the
	// components are the lists). 0x52C744..0x52C758 RemoveMapObject: ecs::ToBeDeleted's generic part
}
} // namespace openblack::ecs::fish_farms
