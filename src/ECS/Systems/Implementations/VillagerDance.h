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
#include <optional>

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager dancing in a dance a script put it in: the scripts' dance state (IN_DANCE, 5), in which it faces as its
/// group's move has it and walks to its place in its group's shape when it isn't there, and the walk to that place
/// (MOVE_TO_DANCE_POS, 154), which follows the place as the group moves and goes back to the dance once there
namespace openblack::ecs::villager_dance
{

/// One step of the walk to a point, and the state the walk was for once there: the walk's result (0xA there)
using WalkStep = std::function<uint32_t(components::LivingAction&)>;

/// Each turn in the scripts' dance: in no group, nothing. Else it turns as its group's move has it and, away from its
/// place, sets out for it (round what is in the way when more than 4 m off), its top state the walk to its place while
/// the dance stays the state it works towards, and takes the walk's first step this turn. Always 1
uint32_t InDance(components::LivingAction& action, const WalkStep& walk);
/// The same in any dance state: walking off to its place, it comes back to that state there. `move` dances that move,
/// by its number in the dance files, in place of its group's. A dancer that worships in a creature's dance gives its town
/// belief in the creature's player (villager_creature_dance::WorshipCreature)
uint32_t PerformDance(components::LivingAction& action, const WalkStep& walk, VillagerStates danceState,
                      std::optional<uint32_t> move = std::nullopt);
/// Walking to its place: in no group it stays where it is (0). A group that moved this turn moves the walk's goal to
/// the place's new position (the walk itself goes on as set out); then the walk's step, back to the dance once there
uint32_t MoveToDancePos(components::LivingAction& action, const WalkStep& walk);
/// Going into the scripts' dance: always accepted (1)
uint32_t EnterInDance(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Leaving the scripts' dance (villager_script_rules::ExitDance): for another of the script's states it stays in its
/// dance and may go (1); else it leaves its dance, and may go (1) only where the script lets it, else 0
uint32_t ExitInDance(components::LivingAction& action, VillagerStates next);

/// The top state written straight as the walk to its place in the dance: the state it works towards stays as it was,
/// the turns in the state start again, and nothing else is told (no exit, entry, clip, speed or town)
void SetWalkingToPlace(components::LivingAction& action);

/// The clip a villager plays in a dance, `walkClip` when its group's move has it walk; `random` the game's numbers,
/// drawn only by the dance of the men and women. In no group, the stand
[[nodiscard]] int32_t DanceClip(entt::entity villager, int32_t walkClip, const std::function<uint32_t(uint32_t)>& random);
/// A dancer whose group starts a move plays its top state's clip again: a new one takes over in time with the dance.
/// Once its clip has played through since its state changed, the clip starts over and the turns in its state are
/// counted from 0 again. Nothing for anything that isn't a villager
void PlayClipAgain(entt::entity villager);

} // namespace openblack::ecs::villager_dance
