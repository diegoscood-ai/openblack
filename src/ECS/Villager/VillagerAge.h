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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"

namespace openblack
{
struct GVillagerInfo;
}

// A villager's age kept as its birth turn, like Living +0xA0 (docs/bw1-notes/villagers.md), and what the age does
// (Villager.cpp / VillagerHome.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V4_spec.md §7, disassembly
// dev\tmp_dis\aldeanos\v4\age.txt): the child that grows up, the scale for the age, the old age's death and the
// pregnancy's count down (WomanSpecial). The births are V14.
namespace openblack::ecs::villager
{

/// GGameInfo +0xC (0xD01A04): turns in a year, ftol(1500.0f) from 0x8DF8D8 in GGameInfo::GGameInfo 0x557730
inline constexpr uint32_t k_TurnsPerYear = 1500;

/// Living::GetAge 0x5ECAF0: (turn - birthTurn) / 1500, an unsigned division (`xor edx, edx; div`, 0x5ECB01)
[[nodiscard]] constexpr uint32_t AgeFromBirthTurn(int32_t birthTurn, uint32_t turn)
{
	return (turn - static_cast<uint32_t>(birthTurn)) / k_TurnsPerYear;
}

/// Living::SetAge 0x5ED2C0: birthTurn = turn - age * 1500 (`imul`, `sub`, 0x5ED2C5..0x5ED2D6)
[[nodiscard]] constexpr int32_t BirthTurnForAge(uint32_t age, uint32_t turn)
{
	return static_cast<int32_t>(turn - age * k_TurnsPerYear);
}

// ---- the pure layer ----------------------------------------------------------------------------------------------

/// CheckChildGrownUp 0x75105F: GetAge >= grownUpAge (+0x138), unsigned (jb)
[[nodiscard]] bool GrownUp(uint32_t age, const GVillagerInfo& info);
/// The age a child grows up to: max(grownUpAge, 18) (0x751079 `cmp eax, 0x12; jae`)
[[nodiscard]] uint32_t GrownUpAge(const GVillagerInfo& info);
/// CheckChildGrownUp 0x7510D6..0x7510F1: the rescale turn of a child: turn % (TimeScale >> 2 = 375) == 0
[[nodiscard]] bool RescaleTurn(uint32_t turn);
/// CheckDeathFromOldAge 0x760CBE..0x760D1A: n = ftol(r^3 x (retirementAge - oldAge)) (the two fmul of POWER's loop,
/// then fild qword, fmulp), the GameRand's range
[[nodiscard]] uint32_t OldAgeRange(float r, uint32_t oldAge, uint32_t retirementAge);
/// 0x760D2C..0x760D3D: age + d > retirementAge (unsigned, jbe) -> dies
[[nodiscard]] bool OldAgeDies(uint32_t age, uint32_t d, uint32_t retirementAge);
/// Villager::InitialiseScale 0x74FB80: a child (age < grownUpAge) ageToScale[age - 1] (+0x2E4 + 4 age: at age 0 the
/// dword before the table, dancingSpeed, read as a float), an adult 0.9 (0x3F666666)
[[nodiscard]] float InitialScaleForAge(const GVillagerInfo& info, uint32_t age);
/// Villager::SetScaleForAge 0x752A90 from the scale `current`: a child current + GameFloatRand((ageToScale[age + 1] -
/// current) x 0.75) (Villager.cpp 0x92F); an adult t = (0.05 - GameFloatRand(0.1)) + 1 (0x933), current < t ?
/// (0.05 - GameFloatRand(0.1)) + 1 : current. Draws the game's synced GameFloatRand
[[nodiscard]] float ScaleForAge(const GVillagerInfo& info, uint32_t age, float current);

// ---- the villager ------------------------------------------------------------------------------------------------

/// Villager::CheckChildGrownUp 0x751050 (the periodic check, children only): grown up -> flags &= ~8, SetAge(max(13,
/// 18)) (vt +0x8D4: no mesh change, the age is already >= 13), the abode's ChildToAdult (or the town's), and
/// ChildBecomesAdult (its result); else on a rescale turn SetScaleForAge(GetAge()). The result
uint32_t CheckChildGrownUp(entt::entity villager);
/// Villager::ChildBecomesAdult 0x757F10: mother (+0x100) = 0; CheckNeedNewAbode; SetTopState(234 GO_HOME_AND_CHANGE); 1
uint32_t ChildBecomesAdult(entt::entity villager);
/// State 115 CHILD_BECOMES_ADULT: ChildBecomesAdult (only a script sets 115: (inferido) no `push 0x73` was searched)
uint32_t ChildBecomesAdultState(components::LivingAction& action);
/// Villager::SetAge 0x7528C0 (vt +0x8D4) after the creation: VillagerCore's SetAge with the original's meshes (only when
/// the age crosses grownUpAge) and InitialiseScale + SetScaleForAge
uint32_t SetAgeAndScale(entt::entity villager, uint32_t age);
/// Villager::InitialiseScale 0x74FB80 / SetScaleForAge 0x752A90 on the villager's Transform scale (Object +0x120 /
/// +0x124 GetScale / SetScale: the uniform scale)
void InitialiseScale(entt::entity villager, uint32_t age);
void SetScaleForAge(entt::entity villager, uint32_t age);
/// Villager::CheckDeathFromOldAge 0x760CA0: age > oldAge (60) -> r = GameFloatRand(1) (VillagerHome.cpp 0x258), d =
/// GameRand(OldAgeRange(r)) (0x25A); dies -> VillagerDead(9 OLD_AGE, no player, info.life (+0x128), 1); 1. Else 0
bool CheckDeathFromOldAge(entt::entity villager);
/// Villager::IsPregnant 0x752210: info sex (+0x1F8) == 1 and +0xF8 != 0
[[nodiscard]] bool IsPregnant(entt::entity villager);
/// Villager::WomanSpecial 0x752240: pregnant and not controlled by a script: +0xF8 -= GetGameTurnsSinceLastChecked
/// (16 bits); <= 0 -> HousewifeStartsGivingBirth (its result). Else 0
uint32_t WomanSpecial(entt::entity villager);
/// Villager::HousewifeStartsGivingBirth 0x7621A0: +0xF8 = 0 (0x7621A6), then TODO(V14): the counter
/// ftol(GameRand(ftol(TimeScale)) + 0.25 TimeScale + 1), SetTopState(111) and HousewifeGivingBirth 0x762430 (110 / 111,
/// ChildBorn) are V14. Unreachable in V4: nobody gets pregnant (CheckGetPregnantAtHome is neutral). 0
uint32_t HousewifeStartsGivingBirth(entt::entity villager);
} // namespace openblack::ecs::villager
