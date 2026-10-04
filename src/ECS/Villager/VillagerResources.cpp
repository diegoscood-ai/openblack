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
#include <string>
#include <utility>

#include <fmt/format.h>

#include "ECS/CarriedProps.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "InfoConstants.h"
#include "Locator.h"

// Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerResources.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// Villager::ExitBuilding 0x7597B0, the exit FUN_00753140 compares with (0x753162)
constexpr uint32_t k_ExitBuilding = 0x7597B0;
/// CARRIED_OBJECT (CarriedObject::Init 0xC5E19C): 1 NONE, 6 BAG, 12 WOOD, 13..15 TREE_1..3; 16 is past the carried ones
/// (CreateDroppedResource 0x75095F `cmp eax, 0x10; jge`)
constexpr int32_t k_CarriedNone = 1;
constexpr int32_t k_CarriedBag = 6;
constexpr int32_t k_CarriedWood = 12;
constexpr int32_t k_CarriedTree1 = 13;
constexpr int32_t k_CarriedTree2 = 14;
constexpr int32_t k_CarriedTree3 = 15;
constexpr int32_t k_CarriedEnd = 16;
/// [0x99A100] = 1e-5, CheckSatisfyFoodDesire 0x75A03E / 0x75A04A
constexpr float k_DropOffEpsilon = 1e-5f;
/// 0x43FA0000 = 500, CheckSatisfyFoodDesire 0x75A061: GetDistanceModifier's maximum
constexpr float k_DropOffMaxDistance = 500.0f;
/// [0x8AB274] = 0.75: the lower clamp of both load factors (0x753AA1, 0x753AD1)
constexpr float k_MinLoadFactor = 0.75f;
/// [0x8AA3B4] = 0.5: the town-needs term's upper clamp (0x7539DF)
constexpr float k_MaxTownNeedsTerm = 0.5f;

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

entt::entity TownEntityOf(const Villager& v)
{
	return TownComponentOf(v) != nullptr ? v.town : entt::null;
}

/// The villager's +0x14 as the MapCoords Town::GetTemporaryResourceStorePotOrPos takes (x / z; GetNearestEdgeToPos does
/// not read the altitude)
map_coords::MapCoords MapCoordsOfVillager(entt::entity villager)
{
	const auto me = tq::PosOf(villager);
	return {me.x, me.y, 0.0f};
}

/// MobileWallHug::GetSpeedInMetres 0x60C070 (vt +0x130): ConvertWholeDistanceToMeters(+0x5A), openblack's WallHug::speed
float SpeedInMetres(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->speed : 0.0f;
}

const char* TypeName(ResourceType type)
{
	return type == ResourceType::Food ? "FOOD" : type == ResourceType::Wood ? "WOOD" : "NONE";
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

void TraceCarry(entt::entity villager, const Villager& v, const char* what, ResourceType type, int32_t amount)
{
	if (!TraceOn(villager))
	{
		return;
	}
	const auto* town = TownComponentOf(v);
	Trace(villager, fmt::format("carry: {} {} {} (held {}/{}, town carried {}/{})", what, TypeName(type), amount,
	                            v.resourceHeld.at(0), v.resourceHeld.at(1), town != nullptr ? town->stats.foodCarried : 0.0f,
	                            town != nullptr ? town->stats.woodCarried : 0.0f));
}

DroppedLogFn& DroppedLogOverride()
{
	static DroppedLogFn s_Make;
	return s_Make;
}

TemporaryStoreFn& TemporaryStoreOverride()
{
	static TemporaryStoreFn s_Store;
	return s_Store;
}

/// Town::GetTemporaryResourceStorePotOrPos 0x73E900 (ecs::town_stores), or the tests'
town_stores::TemporaryStore TemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	if (const auto& store = TemporaryStoreOverride(); store)
	{
		return store(town, from, type);
	}
	return town_stores::GetTemporaryResourceStorePotOrPos(town, from, type);
}

/// DropFood 0x7511E0 / DropWood 0x751240 on +0xF4 / +0xF6 (index 0 / 1): n == 0, or n above what it carries (cmp di, ax;
/// jbe: unsigned 16 bits) -> all of it; held -= n; with a town, Town +0x708 / +0x70C -= (float)(n & 0xFFFF)
uint16_t DropHeld(entt::entity villager, size_t index, uint16_t amount)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto held = static_cast<uint16_t>(v->resourceHeld.at(index));
	if (amount == 0 || amount > held)
	{
		amount = held;
	}
	v->resourceHeld.at(index) = static_cast<int16_t>(static_cast<uint16_t>(held - amount));
	if (auto* town = TownComponentOf(*v))
	{
		auto& carried = index == 0 ? town->stats.foodCarried : town->stats.woodCarried;
		carried = carried - static_cast<float>(amount);
	}
	TraceCarry(villager, *v, "drop", index == 0 ? ResourceType::Food : ResourceType::Wood, amount);
	return amount;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

HeldResource HeldLarger(int16_t food, int16_t wood)
{
	const auto f = static_cast<uint16_t>(food);
	const auto w = static_cast<uint16_t>(wood);
	// 0x751574: type = -1; 0x75157A..0x751599: cmp word +0xF4, +0xF6; jbe; else FOOD and +0xF4
	if (f > w)
	{
		return {ResourceType::Food, f};
	}
	// 0x75159C..0x7515B0: +0xF6 != 0 -> WOOD and +0xF6
	if (w != 0)
	{
		return {ResourceType::Wood, w};
	}
	// 0x7515B3: 0, the type left at -1
	return {ResourceType::None, 0};
}

int32_t WoodCarriedObject(uint16_t flags)
{
	// 0x7502A2..0x7502C7: (+0xE0 >> 14): 1 -> 0xD, 2 -> 0xE, 3 -> 0xF, else 0xC
	switch (flags >> Villager::k_TreeTypeShift)
	{
	case 1:
		return k_CarriedTree1;
	case 2:
		return k_CarriedTree2;
	case 3:
		return k_CarriedTree3;
	default:
		return k_CarriedWood;
	}
}

bool IsBuildingExitState(VillagerStates state)
{
	// 0x75314D..0x75318C: the exit member pointer of the row (0xD091B8 + 0x90 x s) == &ExitBuilding 0x7597B0 with zero
	// adjustors (VillagerOriginalFns.h keeps the address only)
	const auto index = static_cast<size_t>(static_cast<uint8_t>(state));
	return index < k_OriginalStateFns.size() && k_OriginalStateFns.at(index).exit == k_ExitBuilding;
}

int32_t CarriedObjectFor(const CarriedInput& in)
{
	// 0x7501AB / 0x7501B3: final 4 IN_SCRIPT or TOP (+0x8C) 0xC8 SCRIPT_PLAY_ANIM -> +0xF1 unchanged
	if (in.finalState == VillagerStates::InScript || in.topState == VillagerStates::ScriptPlayAnim)
	{
		return in.previous;
	}
	// 0x7501C8: +0xF1 = 1 NONE before the tests
	int32_t carried = k_CarriedNone;
	// 0x7501CF..0x7501E1: GetLife() vs LifeWhenCrawlsWounded (+0x380): `test ah, 0x41; jne` skips when <= (or NaN)
	if (in.life > in.lifeWhenCrawlsWounded)
	{
		if (static_cast<int32_t>(in.wood) > static_cast<int32_t>(in.minWoodToShowGraphic))
		{
			// 0x7501E6..0x7501FC: movsx +0xF6 > +0x26C (jle) -> GetWoodCarriedObject
			carried = WoodCarriedObject(in.flags);
		}
		else if (static_cast<int32_t>(in.food) > static_cast<int32_t>(in.minFoodToShowGraphic))
		{
			// 0x750204..0x75021E: movsx +0xF4 > +0x270 (jle) and not FUN_00753140 (a builder's final state) -> 6 BAG
			if (!in.finalIsBuilding)
			{
				carried = k_CarriedBag;
			}
		}
	}
	// 0x750225..0x75026F: Infos[GetFinalState()] +0xEC (0xDB9F54) != 0 -> it
	if (in.rowCarriedFinal != 0)
	{
		carried = in.rowCarriedFinal;
	}
	// 0x750275..0x750293: Infos[TOP] +0xEC != 0 -> it
	if (in.rowCarriedTop != 0)
	{
		carried = in.rowCarriedTop;
	}
	return carried;
}

LoadFactor LoadFactors(int16_t wood, int16_t food, const GVillagerInfo& info, bool trader)
{
	// 0x7539F8..0x753A53: disciple 9 TRADER -> MaxTraderWood / FoodCarried (+0x278 / +0x274), else +0x268 / +0x264,
	// each `fild qword` (the high dword 0: unsigned; exact in a float: at most 500 in info.dat)
	// The x87 runs with a 24-bit precision control (fn_007DEE00, 0x7DEE0D): every step below rounds to float, on the
	// stack too
	const auto maxWood = static_cast<float>(trader ? info.maxTraderWoodCarried : info.maxWoodCarried);
	const auto maxFood = static_cast<float>(trader ? info.maxTraderFoodCarried : info.maxFoodCarried);
	// 0x753A57..0x753A81: (SpeedModWhenFullLoadOfWood (+0x374) + 1) - (movsx +0xF6) / MW, stored as a float
	// (openblack, test-only guard) a capacity of 0 (only the tests' zero-filled infos; info.dat has 150 / 250 / 500 in
	// all 63 records) gives no load term; the original would give 0.75 there (0 / 0 = NaN, then the clamp)
	const float woodLoad = maxWood != 0.0f ? static_cast<float>(wood) / maxWood : 0.0f;
	const float foodLoad = maxFood != 0.0f ? static_cast<float>(food) / maxFood : 0.0f;
	float woodF = (info.speedModWhenFullLoadOfWood + 1.0f) - woodLoad;
	// 0x753A85..0x753A9B: (SpeedModWhenFullLoadOfFood (+0x378) + 1) - (movsx +0xF4) / MF, kept on the x87 stack (float
	// precision, 0x7DEE0D)
	float foodF = (info.speedModWhenFullLoadOfFood + 1.0f) - foodLoad;
	// 0x753A9D..0x753AC9: woodF < 0.75 (0x8AB274) -> 0.75; > 1 -> 1
	woodF = woodF < k_MinLoadFactor ? k_MinLoadFactor : woodF > 1.0f ? 1.0f : woodF;
	// 0x753AD1..0x753AF7: the same for foodF, with 0.75 too (not +0x378)
	foodF = foodF < k_MinLoadFactor ? k_MinLoadFactor : foodF > 1.0f ? 1.0f : foodF;
	return {woodF, foodF};
}

float TownNeedsFactor(float townNeedsSum, const GVillagerInfo& info)
{
	// 0x7539C2..0x7539F4: S / DivisorForTownNeedsSpeedMod (+0x370); < 0 -> 0; > 0.5 -> 0.5; + BaseForTownNeedsSpeedMod
	// (+0x36C, read at 0x7539AE)
	// (openblack, test-only guard) a divisor of 0 (only the tests' zero-filled infos; info.dat has 2.0) gives no term;
	// the original would give 0.5 there (S / 0 = inf, then the clamp). On the x87 stack at float precision (0x7DEE0D)
	const float divisor = info.divisorForTownNeedsSpeedMod;
	float term = divisor != 0.0f ? townNeedsSum / divisor : 0.0f;
	term = term < 0.0f ? 0.0f : term > k_MaxTownNeedsTerm ? k_MaxTownNeedsTerm : term;
	return term + info.baseForTownNeedsSpeedMod;
}

float DropOffFraction(int16_t capacity, uint32_t maxFoodCarried)
{
	// 0x75A031..0x75A058: fild (movsx) capacity; fadd 1e-5; fild qword MaxFoodCarried; fadd 1e-5; fdivp; fsubr 1;
	// fstp float; every step at float precision (24-bit x87 control word, 0x7DEE0D)
	return 1.0f - (static_cast<float>(capacity) + k_DropOffEpsilon) /
	                  (static_cast<float>(maxFoodCarried) + k_DropOffEpsilon);
}

float DropOffScore(int16_t held, uint32_t maxFoodCarried, float distance)
{
	// GetFoodCapacity 0x7514D0: the 16-bit Max - held
	const auto capacity = static_cast<int16_t>(static_cast<uint16_t>(maxFoodCarried) - static_cast<uint16_t>(held));
	// 0x75A061..0x75A07C: GetDistanceModifier(distance, 500) x frac (on the x87 stack: float precision, 0x7DEE0D)
	return gutils::GetDistanceModifier(distance, k_DropOffMaxDistance) * DropOffFraction(capacity, maxFoodCarried);
}

std::optional<DroppedLog> DroppedLogFor(int32_t carriedObject, int16_t wood, uint32_t minWoodToShowGraphic,
                                        float logWoodValue)
{
	// 0x750950..0x750962: c0 = +0xF1; c0 <= 1 (jle) or c0 >= 16 (jge) -> nothing
	if (carriedObject <= k_CarriedNone || carriedObject >= k_CarriedEnd)
	{
		return std::nullopt;
	}
	// 0x750968..0x75097A: movsx +0xF6 <= MinWoodToShowGraphic (+0x26C) (jle) -> nothing
	if (static_cast<int32_t>(wood) <= static_cast<int32_t>(minWoodToShowGraphic))
	{
		return std::nullopt;
	}
	// 0x7509C6..0x7509E7: fild wood (stored as a float, exact); fdivr GetWoodValue (vt +0x664); fstp +0x9C
	return DroppedLog {carriedObject, static_cast<float>(wood) / logWoodValue};
}

int32_t DroppedLogValue(uint32_t woodValue, float multiplier, float scale)
{
	// DeadTree::GetDefaultResource 0x511330: fild qword woodValue; fmul +0x9C; fmul scale; __ftol (truncated); each fmul
	// rounds to float (24-bit x87 control word, 0x7DEE0D; woodValue is exact in a float)
	return static_cast<int32_t>(static_cast<float>(woodValue) * multiplier * scale);
}

// ---- carrying ----------------------------------------------------------------------------------------------------

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
		TraceCarry(villager, *v, "pickup", ResourceType::Food, amount);
		return amount;
	}
	// 0x751435: add word +0xF6, n; 0x75143E..0x75145E: with a town, +0x70C += n; 0x751460..0x751480: the tree type
	v->resourceHeld.at(1) = static_cast<int16_t>(static_cast<uint16_t>(v->resourceHeld.at(1)) + static_cast<uint16_t>(amount));
	if (town != nullptr)
	{
		town->stats.woodCarried = town->stats.woodCarried + static_cast<float>(amount);
	}
	v->flags = static_cast<uint16_t>((v->flags & ~Villager::k_FlagTreeTypeMask) |
	                                 ((static_cast<uint16_t>(treeType) & 3u) << Villager::k_TreeTypeShift));
	TraceCarry(villager, *v, "pickup", ResourceType::Wood, amount);
	return amount;
}

void PickupFood(entt::entity villager, int16_t amount)
{
	// 0x751490: PickupResource(FOOD, n, 0)
	PickupResource(villager, ResourceType::Food, amount, 0);
}

void PickupWood(entt::entity villager, int16_t amount, uint8_t treeType)
{
	// 0x7514B0: PickupResource(WOOD, n, tree)
	PickupResource(villager, ResourceType::Wood, amount, treeType);
}

uint16_t DropFood(entt::entity villager, uint16_t amount)
{
	// 0x7511E6..0x751236 on +0xF4 and Town +0x708
	return DropHeld(villager, 0, amount);
}

uint16_t DropWood(entt::entity villager, uint16_t amount)
{
	// 0x751246..0x751296 on +0xF6 and Town +0x70C. The tree bits of +0xE0 are not cleared (literal)
	return DropHeld(villager, 1, amount);
}

uint16_t DropResource(entt::entity villager, ResourceType type, uint16_t amount)
{
	// 0x7511B4: 1 WOOD -> DropWood; 0x7511C6: 0 FOOD -> DropFood; 0x7511D7: else 0 (also -2)
	if (type == ResourceType::Wood)
	{
		return DropWood(villager, amount);
	}
	if (type == ResourceType::Food)
	{
		return DropFood(villager, amount);
	}
	return 0;
}

int16_t GetFoodCapacity(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x7514D0..0x7514E8: MaxFoodCarried (+0x264) - +0xF4, only ax meaningful (the callers movsx ax)
	const auto& info = InfoOf(villager);
	return static_cast<int16_t>(static_cast<uint16_t>(info.maxFoodCarried) - static_cast<uint16_t>(v->resourceHeld.at(0)));
}

int16_t GetWoodCapacity(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// 0x7514F0..0x751508: MaxWoodCarried (+0x268) - +0xF6
	const auto& info = InfoOf(villager);
	return static_cast<int16_t>(static_cast<uint16_t>(info.maxWoodCarried) - static_cast<uint16_t>(v->resourceHeld.at(1)));
}

uint16_t GetResourceHeld(entt::entity villager, ResourceType& type)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		type = ResourceType::None;
		return 0;
	}
	const auto held = HeldLarger(v->resourceHeld.at(0), v->resourceHeld.at(1));
	type = held.type;
	return held.amount;
}

uint32_t AddResourceToVillager(entt::entity villager, ResourceType type, uint32_t amount, bool poisoned)
{
	if (type == ResourceType::Food)
	{
		// 0x7564D0..0x7564E0: PickupFood(n); 0x7564E5..0x7564F3: the poisoned argument -> SetPoisoned(1)
		PickupFood(villager, static_cast<int16_t>(amount));
		if (poisoned)
		{
			life::TakePoisonedResource(villager);
		}
	}
	else if (type == ResourceType::Wood)
	{
		// 0x7564F8..: PickupWood(n, 0)
		PickupWood(villager, static_cast<int16_t>(amount), 0);
	}
	// `xor eax, eax`: 0 whatever was added (literal)
	return 0;
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

// ---- the carried object ------------------------------------------------------------------------------------------

int32_t GetWoodCarriedObject(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return WoodCarriedObject(v != nullptr ? v->flags : 0);
}

bool FinalStateIsBuilding(entt::entity villager)
{
	// 0x753143: GetFinalState (vt +0xB04)
	return IsBuildingExitState(GetFinalState(villager));
}

int32_t SetStateCarriedObject(entt::entity villager, int32_t previous)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || !Entities().AllOf<LivingAction>(villager))
	{
		return previous;
	}
	const auto& info = InfoOf(villager);
	CarriedInput in;
	in.finalState = GetFinalState(villager);
	in.topState = GetState(villager, Index::Top);
	in.life = v->life;
	in.wood = v->resourceHeld.at(1);
	in.food = v->resourceHeld.at(0);
	in.flags = v->flags;
	in.finalIsBuilding = IsBuildingExitState(in.finalState);
	in.rowCarriedFinal = state_info::CarriedObject(state_info::StateInfo(in.finalState));
	in.rowCarriedTop = state_info::CarriedObject(state_info::StateInfo(in.topState));
	in.previous = previous;
	in.lifeWhenCrawlsWounded = info.lifeWhenCrawlsWounded;
	in.minWoodToShowGraphic = info.minWoodToShowGraphic;
	in.minFoodToShowGraphic = info.minFoodToShowGraphic;
	const auto carried = CarriedObjectFor(in);
	if (carried != previous)
	{
		TraceIf(villager, fmt::format("carried: {} -> {} (final {}, top {})", previous, carried,
		                              static_cast<uint32_t>(in.finalState), static_cast<uint32_t>(in.topState)));
	}
	return carried;
}

// ---- where -------------------------------------------------------------------------------------------------------

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

glm::ivec2 GetResourceDropoffPos(entt::entity villager, ResourceType type)
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
		// 0x753E9B..0x753ECC: Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900: the town's temporary
		// pot of that type (made now if it has none: a side effect, literal) and its edge towards me; the point is
		// returned whole (0x753EB0..0x753EC9)
		const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
		return {store.pos.x, store.pos.z};
	}
	// 0x753ED3..0x753EEB: no town -> my position
	return me;
}

town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	return TemporaryStore(town, from, type);
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

uint32_t AtStructureAddResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t& amount)
{
	// 0x76A3C9..0x76A3EA: p = GetPlayer() (vt +0x1C); the object's own player -> p = 0; status = p ? fn_0064A9C0(p)
	// (its first non-null slot of +0x14[0..17] -> +0x39C) : 0. TODO(players): fn_0064A9C0 is not ported; (inferred) a
	// villager drops into its own town's pit or home, so status is 0 in every V5 path (no GInterfaceStatus)
	// 0x76A3EC..0x76A3FD: edge = object.GetResourceNearestEdge(type, this, 0) (vt +0x8D4)
	const auto edge = GetResourceNearestEdge(object, type, villager);
	// 0x76A41B..0x76A435: MapCoords::IsCloseToEqual(me, edge, GetSpeedInMetres (vt +0x130, MobileWallHug 0x60C070))
	// 0x6053C0: distance <= the speed (not the radius, unlike AtStructureRemoveResource)
	const auto me = tq::PosOf(villager);
	if (tq::GetDistanceInMetres(me, edge) <= SpeedInMetres(villager))
	{
		// 0x76A437..0x76A451: added = object.AddResource(type, n, status, 0, 0, status != 0) (vt +0x9C):
		// object_resources::AddResource (no GInterfaceStatus)
		const uint32_t added = object_resources::AddResource(object, type, amount);
		// 0x76A459: 0 -> 0
		if (added == 0)
		{
			return 0;
		}
		// 0x76A45D..0x76A46C: DropResource(type, added); n = added; 1
		DropResource(villager, type, static_cast<uint16_t>(added));
		amount = added;
		return 1;
	}
	// 0x76A484..0x76A4A8: SetupMoveToWithHug(edge, GetFinalState); n = 0; 0x24
	SetupMoveToWithHug(villager, tq::ToMetres(edge), GetFinalState(villager));
	amount = 0;
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
	// 0x7699BE..0x7699C7: no functional pit and no town -> 0x769B0A SetTopState(fail); 1
	const auto town = TownEntityOf(*v);
	if (town == entt::null)
	{
		TraceIf(villager, "food 34: no functional storage pit, no town -> fail");
		SetTopState(villager, fail);
		return 1;
	}
	// 0x7699CD..0x7699FD: pot = Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900 (its point is not
	// used here); null -> 0x769B0A (only without a town in openblack)
	const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
	if (store.pot == entt::null)
	{
		TraceIf(villager, "food 34: no temporary pot -> fail");
		SetTopState(villager, fail);
		return 1;
	}
	// 0x769A03..0x769A88: p = pot.GetNearestEdgeOfObject(this) (vt +0x834, Object 0x636CD0 -> GetNearestPosOfObject
	// 0x636D30: the pot's and my 2D radii), back to MapCoords (x / z: ftol(x 6553.6); the altitude, which AreWeThere
	// does not read, is dropped). (approximate) without the original's LHPoint round trip (at most 1 MapCoords unit)
	const auto edge = object::GetNearestPosOfObject(store.pot, villager);
	const auto p = tq::ToMetres(glm::ivec2(edge.x, edge.z));
	// 0x769A8F: AreWeThere(p, 0) (vt +0x85C)
	if (AreWeThere(villager, p, 0.0f))
	{
		const uint32_t had = TraceOn(villager) ? object_resources::GetResource(store.pot, type) : 0;
		// 0x769A99..0x769AA3: pot.RemoveResource(type, n, 0, 0) (vt +0xA0); its result is discarded (eax is overwritten
		// at 0x769AA9): the villager gets the whole n even from an empty pot (literal oddity, V5 spec §1.11)
		static_cast<void>(object_resources::RemoveResource(store.pot, type, amount));
		// 0x769AA9..0x769AB8: PickupResource(type, n, pot.GetCarriedTreeType() (vt +0x820, Object 0x402AF0: 0)). No
		// GetResourceFrom: no poison and no food speed-up from a temporary pot (literal)
		PickupResource(villager, type, static_cast<int16_t>(amount), 0);
		TraceIf(villager, fmt::format("pot: {} from 34 took {} (pot had {})", TypeName(type), amount, had));
		// 0x769ABD..0x769AC7: ok != 0 -> SetTopState(ok); 1
		if (ok != VillagerStates::InvalidState)
		{
			SetTopState(villager, ok);
			return 1;
		}
		// 0x769AC9..0x769ADB: SetTopState(fail); 0
		SetTopState(villager, fail);
		return 0;
	}
	// 0x769AE4..0x769AFE: SetupMoveToWithHug(p, GetFinalState); 0x24
	SetupMoveToWithHug(villager, p, GetFinalState(villager));
	return 0x24;
}

// ---- dropping off ------------------------------------------------------------------------------------------------

uint32_t GotoStoragePitForDropOff(entt::entity villager)
{
	// 0x769626..0x769670: GetStoragePit (the town's, else the home) functional (vt +0xD4) ->
	// SetupMoveToOnFootpath(pit, pit.GetArrivePos() (vt +0x104), 0x20 32); 1
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		const auto arrive = abode_queries::GetArrivePos(pit);
		TraceIf(villager, fmt::format("drop 31: {} {} ({}, {}) -> 32", Entities().AllOf<StoragePit>(pit) ? "pit" : "home",
		                              static_cast<uint32_t>(pit), arrive.x, arrive.y));
		SetupMoveToOnFootpath(villager, pit, arrive, VillagerStates::ArrivesAtStoragePitForDropOff);
		return 1;
	}
	// 0x76967A..0x7696A2: GetResourceHeld(&type); type neither 0 FOOD nor 1 WOOD -> SetTopState(0xA3 163); 0
	ResourceType type = ResourceType::None;
	const auto held = GetResourceHeld(villager, type);
	if (type != ResourceType::Food && type != ResourceType::Wood)
	{
		TraceIf(villager, "drop 31: nothing -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 0;
	}
	// 0x7696A9..0x7696C4: SetupMoveToWithHug(GetResourceDropoffPos(type), 0x20 32); 1
	const auto pos = GetResourceDropoffPos(villager, type);
	TraceIf(villager, fmt::format("drop 31: {} {} -> drop-off point ({}, {}) -> 32", TypeName(type), held, pos.x, pos.y));
	SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForDropOff);
	return 1;
}

uint32_t GotoStoragePitForDropOffState(LivingAction& action)
{
	return GotoStoragePitForDropOff(Entities().ToEntity(action));
}

uint32_t ArrivesAtStoragePitForDropOff(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// 0x7696D5..0x7696E7: n = GetResourceHeld(&type) (only the larger of the two: the other stays in hand, literal);
	// 0 -> 0x7697D7 SetTopState(163); 1
	ResourceType type = ResourceType::None;
	const uint32_t held = GetResourceHeld(villager, type);
	if (held == 0)
	{
		TraceIf(villager, "drop 32: none -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		// 0x769708..0x769717: AtStructureAddResource(pit, type, &n, 0)
		uint32_t amount = held;
		const auto r = AtStructureAddResource(villager, pit, type, amount);
		TraceIf(villager, fmt::format("drop 32: held {} {} -> pit {} +{} (r={:#x})", TypeName(type), held,
		                              static_cast<uint32_t>(pit), amount, r));
		// 0x76971C..0x76971F: 0x24 (VILLAGER_STATE_GO_HOME reused as "on the way") -> 0x7697E6: 1
		if (r == 0x24)
		{
			return 1;
		}
		// 0x769725..0x769748: added or refused, SetupMoveToOnFootpath(pit, pit.GetArrivePos(), 0xA3 163); 1
		SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x76974D..0x769756: without a town -> 0x7697D7 SetTopState(163); 1
	const auto* v = VillagerOf(villager);
	const auto town = v != nullptr ? TownEntityOf(*v) : entt::null;
	if (town == entt::null)
	{
		TraceIf(villager, "drop 32: no pit, no town -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x769758..0x769790: pot = Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900; null -> 163; 1
	const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
	if (store.pot == entt::null)
	{
		TraceIf(villager, "drop 32: no temporary pot -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x769792..0x7697A5: AreWeThere(pos, 0) (vt +0x85C)
	const auto pos = tq::ToMetres(glm::ivec2(store.pos.x, store.pos.z));
	if (AreWeThere(villager, pos, 0.0f))
	{
		// 0x7697A7..0x7697BD: pot.AddResource(type, n, 0, 0, 0, 0) (vt +0x9C, PotStructure 0x66ED70:
		// object_resources::AddResource); the result is ignored
		static_cast<void>(object_resources::AddResource(store.pot, type, held));
		// 0x7697C3..0x7697FD: type 0 FOOD -> DropFood(n), else DropWood(n); then SetTopState(163); 1
		if (type == ResourceType::Food)
		{
			DropFood(villager, static_cast<uint16_t>(held));
		}
		else
		{
			DropWood(villager, static_cast<uint16_t>(held));
		}
		TraceIf(villager, fmt::format("drop 32: held {} {} -> pot {} +{} -> 163", TypeName(type), held,
		                              static_cast<uint32_t>(store.pot), held));
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// 0x7697FF..0x769817: SetupMoveToWithHug(pos, GetFinalState()); 1
	TraceIf(villager, fmt::format("drop 32: walking to the pot {}", static_cast<uint32_t>(store.pot)));
	SetupMoveToWithHug(villager, pos, GetFinalState(villager));
	return 1;
}

// ---- dropped resources -------------------------------------------------------------------------------------------

void CreateDroppedResource(entt::entity villager, std::optional<glm::vec3> velocity, std::optional<glm::vec3> angular,
                           std::optional<glm::vec3> extra)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// 0x750950: c0 = +0xF1 (SkeletalAnimation::carriedObject; 1 NONE without one)
	const auto* animation = Entities().TryGet<const SkeletalAnimation>(villager);
	const int32_t carried = animation != nullptr ? animation->carriedObject : k_CarriedNone;
	// 0x7509D7: the new log's GetWoodValue (vt +0x664, DeadTree 0x511AD0 = life x woodValue x scale^3) with GTreeInfo
	// 0xDA49D8 (Pine): life 1 (fn_00510BB0's 4th argument, 0x7509B0 -> the ctor's SetLife) and scale 1
	const auto& pine = Locator::infoConstants::value().tree.at(static_cast<size_t>(TreeInfo::Pine));
	const auto logWoodValue = static_cast<float>(pine.woodValue);
	const auto& info = InfoOf(villager);
	const auto log = DroppedLogFor(carried, v->resourceHeld.at(1), info.minWoodToShowGraphic, logWoodValue);
	if (!log)
	{
		// 0x750956..0x75097A -> 0x750A9A: nothing (and no DropWood)
		TraceIf(villager, "dropped log: none");
		return;
	}
	// 0x750993..0x7509C1: mesh = CarriedObject 0xC5E19C[c0] -> ResolveLoad (vt +0xF8); DeadTree::Create fn_00510BB0(my
	// pos (+0x14), GTreeInfo 0xDA49D8, 0, scale 1.0, angle pi/2 (0x3FC90FDB), 0, 0, mesh); +0x9C = the multiplier;
	// 0x750A05..0x750A89: po = log.InitialisePhysics(a ? a : 0, a ? (b ? b : 0) : 0, 0, 1, 0) (vt +0x784); with a body,
	// po +0x90 = *c (if c), po +0x1D8 |= 0x10, PhysOb::AdjustToGroundLevel(po +0x28, 0, 1) 0x7FCB80 and
	// PhysicsObject::RaiseUntilNotIntersecting 0x644800 (ecs::CreateDroppedLog, PhysicsObjects::AddDroppedObject)
	const auto* transform = Entities().TryGet<const Transform>(villager);
	const auto pos = transform != nullptr ? transform->position : glm::vec3(0.0f);
	const auto mesh = CarriedObjectMesh(log->carriedObject);
	const glm::vec3 a = velocity.value_or(glm::vec3(0.0f));
	const glm::vec3 b = velocity && angular ? *angular : glm::vec3(0.0f);
	TraceIf(villager, fmt::format("dropped log: wood {} carried {} mesh {} multiplier {:.9f} value {}", v->resourceHeld.at(1),
	                              log->carriedObject, mesh, log->multiplier,
	                              DroppedLogValue(pine.woodValue, log->multiplier, 1.0f)));
	if (const auto& make = DroppedLogOverride(); make)
	{
		make(pos, mesh, log->multiplier, a, b, extra);
	}
	else
	{
		const auto entity = CreateDroppedLog(pos, mesh, log->multiplier);
		physics::PhysicsObjects::AddDroppedObject(entity, a, b, extra);
	}
	// 0x750A91..0x750A95: DropWood(0): all the wood
	DropWood(villager, 0);
}

// ---- test hooks --------------------------------------------------------------------------------------------------

void SetDroppedLogForTests(DroppedLogFn make)
{
	DroppedLogOverride() = std::move(make);
}

void SetTemporaryStoreForTests(TemporaryStoreFn store)
{
	TemporaryStoreOverride() = std::move(store);
}
} // namespace openblack::ecs::villager
