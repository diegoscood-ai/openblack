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

#include <functional>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::villager
{
/// One row of the villager state table (the original's at 0xD09198, 0x90 bytes a state; the rows openblack has are in
/// LivingActionSystem.cpp k_VillagerStateTable). An empty slot is "no function", as in the original: the callers then
/// act as if it had returned 1 (Villager::CallExitStateFunction 0x752320, CallEntryStateFunction 0x7523D0).
struct VillagerStateTableEntry
{
	/// +0x00: the state function, called each turn while it is the TOP state (Villager::CallState 0x7521D0)
	std::function<uint32_t(components::LivingAction&)> state = nullptr;
	/// +0x10: the entry, with the final state from before the change and the state entered. 1 = accepted (the caller
	/// sets the state), 0x23 = accepted and the function set the states itself, anything else = refused
	std::function<uint32_t(components::LivingAction&, VillagerStates final, VillagerStates next)> entryState = nullptr;
	/// +0x20: the exit, with the state that follows. 1 = it may leave
	std::function<uint32_t(components::LivingAction&, VillagerStates next)> exitState = nullptr;
	std::function<bool(components::LivingAction&)> saveState = nullptr; ///< +0x30
	std::function<bool(components::LivingAction&)> loadState = nullptr; ///< +0x40
	std::function<bool(components::LivingAction&)> field0x50 = nullptr; ///< +0x50 (AlwaysReactToTownEmergency...)
	std::function<bool(components::LivingAction&)> field0x60 = nullptr; ///< +0x60 (the state's clip function)
	/// +0x70, the into / out-of clip function: not used, the clips go by k_StateAnimFns (ECS/VillagerAnimationTable.h)
	std::function<int(components::LivingAction&)> transitionAnimation = nullptr;
	std::function<bool(components::LivingAction&)> validate = nullptr; ///< +0x80 (ProcessState, result unused)
};
} // namespace openblack::ecs::villager
