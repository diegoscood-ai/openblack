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

/// A villager's clips: the clip of the state it is in, and setting a clip on it. The rules are villager_animation's;
/// this applies them to the villager. The clips of a change of state are ECS/VillagerAnimations'.
namespace openblack::ecs::villager_animate
{

/// The clip its top state plays now (choosing it, with any random draws, and what it carries first): negative to keep
/// the one it has, the hidden clip to be out of sight. The stand for no state
[[nodiscard]] int32_t StateClip(entt::entity villager);
/// Plays a clip (nothing for a negative one, one it already plays, or a villager without a clip yet), from the start
/// when asked to, unless it dances in time with others
void SetAnim(entt::entity villager, int32_t clip, bool restart);
/// Plays its top state's clip the same way (nothing for a villager without a clip yet, and then no clip is chosen)
void PlayStateClip(entt::entity villager, bool restart);
/// Plays its top state's clip, from the start when it is a new one (giving it its clip component first when it has
/// none)
void SetStateAnim(entt::entity villager);
/// Gives a new villager its clip component and its first clip, as it is made: the clip of the state it is made in
/// (just created or drowning), from the start, after what it carries is worked out. Nothing is drawn
void SetFirstClip(entt::entity villager);
/// A clip a script sets over its state's: from the start when it is a new one (nothing for a negative one or a
/// villager without a clip yet)
void OverrideAnim(entt::entity villager, int32_t clip);
/// The clip it plays now, -1 for none
[[nodiscard]] int32_t CurrentClip(entt::entity villager);

/// Whether its top state keeps it out of sight
[[nodiscard]] bool IsHiddenByState(VillagerStates state);

} // namespace openblack::ecs::villager_animate
