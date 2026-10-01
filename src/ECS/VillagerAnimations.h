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

/// Villager::CallOutofAnimationFunction (0x756620), called before the new state's entry (Living::SetTopState
/// 0x5F2900): -1 if `next` has no out-of clip (state table file 0xF0, 0xDB9F68), else the current TOP's into / out-of
/// function with (0, next); a clip sets the flags 0x1800 (Villager +0xE1 |= 0x18, here transitionFlags).
int32_t VillagerCallOutOfAnimation(entt::entity villager, VillagerStates next);

/// The tail of Living::SetTopState (0x5F2917) and of Living::SetCurrentAndDestinationState (0x5F29BB): the state's
/// speed (Villager::SetStateSpeed, called with no test; its skips are its own, see SetVillagerStateSpeed), then the
/// out-of clip if there was one, else SetStateAnim (0x5ECB10) and CallIntoAnimationFunction (0x756590: the TOP's
/// function with (1, entered); a clip sets 0x800 and clears 0x1000). `entered` is SetTopState's s (0x5F2947) or
/// SetCurrentAndDestinationState's destination d, not its current state c (0x5F29EB pushes ebx = arg 2); the
/// out-of clip of SetCurrentAndDestinationState is also VillagerCallOutOfAnimation(villager, d) (0x5F299C).
void VillagerApplyStateClips(entt::entity villager, VillagerStates entered, int32_t out);

/// Both halves at once, for the changes that bypass the exit and entry functions (LivingActionSystem::VillagerSetState
/// with skipTransition: the hand, the physics, the animals, LANDED). `previous` is the TOP
/// before the change; the new TOP is already set.
void OnVillagerStateChanged(entt::entity villager, VillagerStates previous, VillagerStates next);

/// Villager::ProcessState (0x74FF70): while an into / out-of clip plays the state logic waits. Returns true if it must
/// wait this turn; once the clip is over it switches to the state's own clip (FinishedIntoOutOfAnimation 0x750060).
bool VillagerWaitsForTransition(entt::entity villager, uint16_t turnsSinceStateChange);

/// Living::SetAnim(GetAnimId(), n) (0x5ECBA0, from Living::SetAnim(n) 0x5ECB80): the state's clip if it is another
/// one; `reset` (n != 0 and not dancing) starts it from the beginning
void VillagerSetStateClip(entt::entity villager, bool reset);

/// Living::IsReadyForNewAnimation (0x5EC960): the current clip has played once (turns in the state * 100 ms)
bool VillagerAnimationDone(entt::entity villager, uint16_t turnsSinceStateChange);

/// A new top state for a villager from outside its state logic (the hand, the physics): IN_HAND when picked up,
/// FLYING when thrown, LANDED when it comes to rest. The villager stops walking.
void SetVillagerState(entt::entity villager, VillagerStates state);

/// Per frame: gives new villagers their clip and feeds the walk sync (the speed of the moving states).
void UpdateVillagerAnimations();

} // namespace openblack::ecs
