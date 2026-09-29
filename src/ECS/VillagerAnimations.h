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

#include "Enums.h"

namespace openblack::ecs
{

/// Which clip a villager plays, like the original (docs/bw1-notes/animation.md): the state's clip from info.dat or its
/// hard-coded animation function, the into / out-of clips of a state change, and the walk clip synced to the ground
/// covered.

/// Villager::GetAnimId (0x750110): the clip for the villager's current state (an ANM_ index; -4 = not drawn)
int32_t VillagerAnimId(entt::entity villager);

/// Living::SetTopState's clip part (0x5F28E0): the previous state's out-of clip, else the new state's clip and then
/// its into clip. Called when the top state changes.
void OnVillagerStateChanged(entt::entity villager, VillagerStates previous, VillagerStates next);

/// Villager::ProcessState (0x74FF70): while an into / out-of clip plays the state logic waits. Returns true if it must
/// wait this turn; once the clip is over it switches to the state's own clip (FinishedIntoOutOfAnimation 0x750060).
bool VillagerWaitsForTransition(entt::entity villager, uint16_t turnsSinceStateChange);

/// Living::IsReadyForNewAnimation (0x5EC960): the current clip has played once (turns in the state * 100 ms)
bool VillagerAnimationDone(entt::entity villager, uint16_t turnsSinceStateChange);

/// A new top state for a villager from outside its state logic (the hand, the physics): IN_HAND when picked up,
/// FLYING when thrown, LANDED when it comes to rest. The villager stops walking.
void SetVillagerState(entt::entity villager, VillagerStates state);

/// Per frame: gives new villagers their clip and feeds the walk sync (the speed of the moving states).
void UpdateVillagerAnimations();

} // namespace openblack::ecs
