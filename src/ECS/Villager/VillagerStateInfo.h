/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

// GVillagerStateTableInfo (info.dat, one row per villager state; GVillagerStateTableInfo::Infos 0xDB9E68, 0x114 bytes a
// row) with the names the original's readers give its fields (docs/bw1-notes/villagers.md §Tabla de estados). The
// struct in InfoConstants.h keeps its field0x.. names (other sessions use them); these accessors only name them. The
// offset in a comment is the file's; the disassembly reads it at +0x10 (the row's header): "memoria".

namespace openblack::ecs::villager::state_info
{
/// file 0x00 (memory 0x10): the state's clip (ANM_, 385 P_STAND; -4 not drawn). Villager::GetAnimId 0x750110
inline int32_t Clip(const GVillagerStateTableInfo& s)
{
	return static_cast<int32_t>(s.field0x0);
}
/// 0x04 (0x14): the town desire the state serves (TownDesireInfo, -1 none). Villager::AdjustTownModifier 0x753560
inline int ServedDesire(const GVillagerStateTableInfo& s)
{
	return s.field0x4;
}
/// 0x08 (0x18): how much of it. AdjustTownModifier 0x753560 (0xDB9E80)
inline float ServedDesireAmount(const GVillagerStateTableInfo& s)
{
	return s.field0x8;
}
/// 0x0C (0x1C): a final state. Villager::GetFinalState 0x751DD0 (0xDB9E84), Villager::SetState 0x753690
inline bool IsFinal(const GVillagerStateTableInfo& s)
{
	return s.isFinalState != 0;
}
/// 0x10 (0x20): never kept in PREVIOUS. Villager::SetState(2, s) 0x7536A2 (0xDB9E88)
inline bool NotStoredAsPrevious(const GVillagerStateTableInfo& s)
{
	return s.field0x10 != 0;
}
/// 0x14 (0x24): a moving state. Villager::CheckEveryTime 0x750439, IsMovingForAnimation
inline bool IsMoving(const GVillagerStateTableInfo& s)
{
	return s.field0x14 != 0;
}
/// 0x18 (0x28)
inline bool IsScriptState(const GVillagerStateTableInfo& s)
{
	return s.isScriptState != 0;
}
/// 0x1C (0x2C)
inline bool IsScriptInterruptable(const GVillagerStateTableInfo& s)
{
	return s.isScriptInterruptableState != 0;
}
/// 0x20 (0x30): the state PopFromPrevious 0x751E50 resumes
inline VillagerStates ResumeState(const GVillagerStateTableInfo& s)
{
	return static_cast<VillagerStates>(s.field0x20);
}
/// 0x24 (0x34): the speed group entry. Villager::SetStateSpeed 0x753760
inline uint32_t SpeedGroup(const GVillagerStateTableInfo& s)
{
	return s.field0x24;
}
/// 0xA8 (0xB8): 1 / 3 / 4 (IsVillagerAvailable tests & 1). GetVillagerAvailableState 0x751F40
inline int AvailableState(const GVillagerStateTableInfo& s)
{
	return s.field0xa8;
}
/// 0xB0 (0xC0): no name yet (FUN_00751F70)
inline uint32_t Field0xB0(const GVillagerStateTableInfo& s)
{
	return s.field0xb0;
}
/// 0xB4 (0xC4): HousewifeCalledToMakeDinner
inline uint32_t DinnerInterrupt(const GVillagerStateTableInfo& s)
{
	return s.field0xb4;
}
/// 0xB8 (0xC8): IsReactiveState, ExitReaction
inline bool IsReactive(const GVillagerStateTableInfo& s)
{
	return s.field0xb8 != 0;
}
/// 0xC0 (0xD0): ExitAtHome 0x761B40 (0 -> LeaveHome)
inline bool StaysAtHomeOnExit(const GVillagerStateTableInfo& s)
{
	return s.field0xc0 != 0;
}
/// 0xC4 (0xD4): it may start with a pause. Villager::CanPauseForASecond 0x752120 (0xDB9F3C)
inline bool CanPauseForASecond(const GVillagerStateTableInfo& s)
{
	return s.field0xc4 != 0;
}
/// 0xC8 (0xD8): IsInterestedInFoodObject
inline float FoodInterest(const GVillagerStateTableInfo& s)
{
	return s.field0xc8;
}
/// 0xCC (0xDC): IsInterestedInWoodObject
inline float WoodInterest(const GVillagerStateTableInfo& s)
{
	return s.field0xcc;
}
/// 0xD0 (0xE0): CheckHungry
inline bool InterruptWhenHungry(const GVillagerStateTableInfo& s)
{
	return s.field0xd0 != 0;
}
/// 0xD4 (0xE4): CheckHungry
inline bool InterruptWhenStarving(const GVillagerStateTableInfo& s)
{
	return s.field0xd4 != 0;
}
/// 0xD8 (0xE8): it does not go home when hurt. CheckEveryTime 0x75058D, ExitGetFoodAtWorship
inline bool NoGoHomeWhenHurt(const GVillagerStateTableInfo& s)
{
	return s.field0xd8 != 0;
}
/// 0xDC (0xEC): the carried object. SetStateCarriedObject 0x7501A0
inline int CarriedObject(const GVillagerStateTableInfo& s)
{
	return s.field0xdc;
}
/// 0xE0 (0xF0): IsDoingSomethingInteresting
inline uint32_t Interesting(const GVillagerStateTableInfo& s)
{
	return s.field0xe0;
}
/// 0xE4 (0xF4): the periodic checks run in this state. CheckEveryTime 0x7504A1
inline bool DoPeriodicChecks(const GVillagerStateTableInfo& s)
{
	return s.field0xe4 != 0;
}
/// 0xE8 (0xF8): it goes home when hurt. CheckEveryTime 0x75057A
inline bool GoHomeWhenHurt(const GVillagerStateTableInfo& s)
{
	return s.field0xe8 != 0;
}
/// 0xEC (0xFC): IsAvailableForReaction
inline bool AvailableForReaction(const GVillagerStateTableInfo& s)
{
	return s.field0xec != 0;
}
/// 0xF0 (0x100): no out-of clip when entering it. CallOutofAnimationFunction 0x75663E (0xDB9F68)
inline bool NoOutOfClip(const GVillagerStateTableInfo& s)
{
	return s.field0xf0 != 0;
}
/// 0xF4 (0x104): Villager::EndPhysics
inline uint32_t EndPhysicsFlag(const GVillagerStateTableInfo& s)
{
	return s.field0xf4;
}
/// 0xF8 (0x108): life lost each turn in the state. CheckEveryTime 0x750442 / 0x75048F (0xDB9F70)
inline float LifeDrainPerTurn(const GVillagerStateTableInfo& s)
{
	return s.field0xf8;
}
/// 0xFC (0x10C): its help text (6116 + n)
inline uint32_t HelpText(const GVillagerStateTableInfo& s)
{
	return s.field0xfc;
}
/// 0x100 (0x110)
inline uint32_t QueryText(const GVillagerStateTableInfo& s)
{
	return s.field0x100;
}

/// GVillagerStateTableInfo::Infos[s]: the state index is a byte in the original (Living +0x8C); info.dat has rows
/// 0..254, so 255 reads row 254 (aproximado: no state 255 exists)
inline const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto index = std::min<size_t>(static_cast<uint8_t>(state), table.size() - 1);
	return table.at(index);
}
} // namespace openblack::ecs::villager::state_info
