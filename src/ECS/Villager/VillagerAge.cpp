/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAge.h"

#include <bit>

#include <fmt/format.h>

#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// Villager.cpp / VillagerHome.cpp / VillagerHousewife.cpp of runblack.exe W120 (VillagerAge.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// SetAge 0x752907 / CheckChildGrownUp 0x751079: an adult is at least 18 (0x12)
constexpr uint32_t k_AdultAge = 18;
/// InitialiseScale 0x74FBA6: an adult's 0.9 (0x3F666666)
constexpr float k_AdultInitialScale = 0.9f;
/// SetScaleForAge 0x752ABA: 0.75 (0x8AB274) of the way to the next age's scale
constexpr float k_ChildScaleStep = 0.75f;
/// SetScaleForAge 0x752AFF / 0x752B0B / 0x752B16: GameFloatRand(0.1 [0x3DCCCCCD]), 0.05 [0x8AC3F4] - it, + 1 [0x8AA390]
constexpr float k_AdultScaleRand = 0.1f;
constexpr float k_AdultScaleBase = 0.05f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// ageToScale (GVillagerInfo memory +0x2E8): index i is ageToScale[i]; i = -1 reads the dword before (+0x2E4,
/// dancingSpeed) as a float (InitialiseScale 0x74FB90 at age 0)
float AgeToScaleAt(const GVillagerInfo& info, int32_t i)
{
	if (i < 0)
	{
		return std::bit_cast<float>(info.dancingSpeed);
	}
	const auto& table = info.ageToScale.values;
	// (openblack, guard) past the 20 floats the original reads the next fields: never for an age below grownUpAge 13
	return static_cast<size_t>(i) < table.size() ? table.at(static_cast<size_t>(i)) : table.back();
}

float ScaleOf(entt::entity villager)
{
	const auto* t = Entities().TryGet<const Transform>(villager);
	return t != nullptr ? t->scale.x : 1.0f;
}

void SetScale(entt::entity villager, float scale)
{
	// Object::SetScale 0x639200 (vt +0x124): the uniform scale
	if (auto* t = Entities().TryGet<Transform>(villager))
	{
		t->scale = glm::vec3(scale);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

bool GrownUp(uint32_t age, const GVillagerInfo& info)
{
	return !(age < info.grownUpAge);
}

uint32_t GrownUpAge(const GVillagerInfo& info)
{
	return info.grownUpAge < k_AdultAge ? k_AdultAge : info.grownUpAge;
}

bool RescaleTurn(uint32_t turn)
{
	// 0x7510D6..0x7510EF: g_game +0x205A40 % ([0xD01A04] >> 2) (unsigned div)
	return turn % (k_TurnsPerYear >> 2u) == 0;
}

uint32_t OldAgeRange(float r, uint32_t oldAge, uint32_t retirementAge)
{
	// 0x760CEA..0x760CF4: the loop of two fmul: r x r x r (24-bit FPU: each one rounded to a float)
	const float r2 = r * r;
	const float r3 = r2 * r;
	// 0x760CF6..0x760D15: fild qword (u32) (retirement - old); fmulp; __ftol
	const auto span = static_cast<float>(static_cast<double>(retirementAge - oldAge));
	return static_cast<uint32_t>(map_coords::FtoL(r3 * span));
}

bool OldAgeDies(uint32_t age, uint32_t d, uint32_t retirementAge)
{
	return age + d > retirementAge;
}

float InitialScaleForAge(const GVillagerInfo& info, uint32_t age)
{
	// 0x74FB88..0x74FBAB: age < grownUpAge (jae) -> ageToScale[age - 1]; else 0.9
	if (age < info.grownUpAge)
	{
		return AgeToScaleAt(info, static_cast<int32_t>(age) - 1);
	}
	return k_AdultInitialScale;
}

float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current)
{
	if (age < info.grownUpAge)
	{
		// 0x752AA4..0x752AE9: (ageToScale[age + 1] - GetScale) x 0.75 (fsubr, fmul, stored as the argument);
		// GameFloatRand (Villager.cpp 0x92F); SetScale(GetScale + it)
		const float gap = AgeToScaleAt(info, static_cast<int32_t>(age) + 1) - current;
		const float range = gap * k_ChildScaleStep;
		const float r = GameFloatRand(range);
		return current + r;
	}
	// 0x752AF5..0x752B1C: t = (0.05 - GameFloatRand(0.1)) + 1 (0x933)
	const float r1 = GameFloatRand(k_AdultScaleRand);
	const float t = (k_AdultScaleBase - r1) + 1.0f;
	// 0x752B20..0x752B2F: GetScale < t (test ah, 1) -> another (0.05 - GameFloatRand(0.1)) + 1 (0x933); else GetScale
	if (current < t)
	{
		const float r2 = GameFloatRand(k_AdultScaleRand);
		return (k_AdultScaleBase - r2) + 1.0f;
	}
	return current;
}

// ---- the villager ------------------------------------------------------------------------------------------------

void InitialiseScale(entt::entity villager, uint32_t age)
{
	SetScale(villager, InitialScaleForAge(InfoOf(villager), age));
}

void SetScaleForAge(entt::entity villager, uint32_t age)
{
	SetScale(villager, ScaleForAge(InfoOf(villager), age, ScaleOf(villager)));
}

uint32_t SetAgeAndScale(entt::entity villager, uint32_t age)
{
	const auto& info = InfoOf(villager);
	const uint32_t turn = CurrentTurn();
	return SetAge(villager, info, age, turn, [villager, &info](uint32_t set) {
		// 0x752919..0x7529B1 / 0x7529B6..0x752A5C: the meshes only when the age crosses grownUpAge (GetAge with the old
		// birth turn): a new child the child meshes (+0x20C / +0x208 / +0x204), a new adult GetDetailMesh(2 / 1 / 0).
		// The skeleton branch (IsSkeleton vt +0x4A0, [0xDA6BC8]) is V12's
		const uint32_t old = GetAge(villager);
		const bool child = set < info.grownUpAge;
		if (child && !(old < info.grownUpAge))
		{
			SetVillagerMeshes(villager, info, true, false);
		}
		else if (!child && old < info.grownUpAge)
		{
			SetVillagerMeshes(villager, info, false, false);
		}
		// 0x752A62 InitialiseScale, 0x752A6D SetScaleForAge
		InitialiseScale(villager, set);
		SetScaleForAge(villager, set);
	});
}

uint32_t CheckChildGrownUp(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	const uint32_t age = GetAge(villager);
	// 0x751059..0x751065: GetAge >= grownUpAge (+0x138)
	if (GrownUp(age, info))
	{
		// 0x751067: flags &= ~8
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagChild);
		// 0x751070..0x751088: SetAge(max(grownUpAge, 18)) (vt +0x8D4)
		const uint32_t to = GrownUpAge(info);
		SetAgeAndScale(villager, to);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: grown {} -> {} (abode {})", age, to,
			                            static_cast<uint32_t>(Entities().Get<const Villager>(villager).abode)));
		}
		// 0x75108E..0x7510A3: an abode -> Abode::ChildToAdult 0x404CC0; else with a town Town::ChildToAdult 0x73AF50
		const auto* again = Entities().TryGet<const Villager>(villager);
		if (again != nullptr && again->abode != entt::null && Entities().Valid(again->abode))
		{
			abode_villagers::ChildToAdult(again->abode, villager);
		}
		else if (again != nullptr && again->town != entt::null && Entities().Valid(again->town))
		{
			town_villagers::ChildToAdult(again->town, villager);
		}
		// 0x7510AA / 0x7510CE: return ChildBecomesAdult
		return ChildBecomesAdult(villager);
	}
	// 0x7510D6..0x751100: turn % 375 == 0 -> SetScaleForAge(GetAge())
	if (RescaleTurn(CurrentTurn()))
	{
		SetScaleForAge(villager, age);
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("age: rescale {} -> {:.4f}", age, ScaleOf(villager)));
		}
	}
	return 0;
}

uint32_t ChildBecomesAdult(entt::entity villager)
{
	// 0x757F13: mother = 0
	if (auto* v = Entities().TryGet<Villager>(villager))
	{
		v->mother = entt::null;
	}
	// 0x757F1D CheckNeedNewAbode; 0x757F24..0x757F2B SetTopState(0xEA 234 GO_HOME_AND_CHANGE); 1
	CheckNeedNewAbode(villager);
	SetTopState(villager, VillagerStates::GoHomeAndChange);
	return 1;
}

uint32_t ChildBecomesAdultState(LivingAction& action)
{
	return ChildBecomesAdult(Entities().ToEntity(action));
}

bool CheckDeathFromOldAge(entt::entity villager)
{
	const auto& info = InfoOf(villager);
	// 0x760CAC..0x760CB8: GetAge <= oldAge (+0x13C, jbe) -> 0
	if (!(GetAge(villager) > info.oldAge))
	{
		return false;
	}
	// 0x760CCD..0x760CDE: GameFloatRand(1) (VillagerHome.cpp 0x258); 0x760D09..0x760D1B: GameRand(n) (0x25A)
	const float r = GameFloatRand(1.0f);
	const uint32_t n = OldAgeRange(r, info.oldAge, info.retirementAge);
	const uint32_t d = GameRand(n);
	// 0x760D2C..0x760D3D: GetAge (again) + d > retirementAge (+0x140)
	const uint32_t age = GetAge(villager);
	const bool dies = OldAgeDies(age, d, info.retirementAge);
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("age: old age {} r={:.6f} n={} d={} -> {}", age, r, n, d, dies ? "die" : "live"));
	}
	if (!dies)
	{
		return false;
	}
	// 0x760D3F..0x760D51: VillagerDead(9 OLD_AGE, 0 (0x760D4B push 0: no player), info +0x128 life, 1)
	VillagerDead(villager, DeathReason::OldAge, std::nullopt, info.life, 1);
	return true;
}

bool IsPregnant(entt::entity villager)
{
	// 0x752210: info +0x1F8 == 1 && +0xF8 != 0
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && InfoOf(villager).sex == SexType::Female && v->pregnancy != 0;
}

uint32_t WomanSpecial(entt::entity villager)
{
	auto* v = Entities().TryGet<Villager>(villager);
	// 0x752243..0x75225A: pregnant, and +0x24 & 0x400 clear (not controlled by a script)
	if (v == nullptr || !IsPregnant(villager) || script_held::IsControlledByScript(villager))
	{
		return 0;
	}
	// 0x75225E..0x752272: +0xF8 -= GetGameTurnsSinceLastChecked (sub word); > 0 (signed jg) -> 0
	const auto turns = GetGameTurnsSinceLastChecked(villager, CurrentTurn());
	v->pregnancy = static_cast<int16_t>(static_cast<uint16_t>(v->pregnancy) - static_cast<uint16_t>(turns));
	if (v->pregnancy > 0)
	{
		return 0;
	}
	// 0x752276: HousewifeStartsGivingBirth (its result)
	return HousewifeStartsGivingBirth(villager);
}

uint32_t HousewifeStartsGivingBirth(entt::entity villager)
{
	// 0x7621A6: +0xF8 = 0
	if (auto* v = Entities().TryGet<Villager>(villager))
	{
		v->pregnancy = 0;
	}
	// TODO(V14): 0x7621AF..0x762208: +0x58 = ftol(GameRand(ftol(TimeScale)) + 0.25 TimeScale + 1) (VillagerHousewife.cpp
	// 0x120), SetTopState(111 HOUSEWIFE_GIVING_BIRTH), HousewifeGivingBirth 0x762430 (ChildBorn). Not ported; no
	// pregnancy can start in V4 (CheckGetPregnantAtHome is neutral, P-2)
	if (TraceOn(villager))
	{
		Trace(villager, "age: HousewifeStartsGivingBirth 0x7621A0 TODO(V14)");
	}
	return 0;
}
} // namespace openblack::ecs::villager
