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

namespace openblack::ecs
{

/// The animals' clips (docs/bw1-notes/animation.md, animals.md): Animal::GetAnimId (0x417FA0) always asks the
/// species' function of the state (g_AnimalStateTable slot 0x60, no info.dat fallback); -1 keeps the clip playing.
/// There are no into / out-of clips for animals.

/// Animal::GetAnimId: the ANM_ index for the animal's top state, -1 = keep the current clip
[[nodiscard]] int32_t AnimalAnimId(entt::entity entity);
/// Living::SetAnim (0x5ECBA0): cut to that clip (from 0 with `reset`); the same clip is not restarted
void SetAnimalAnim(entt::entity entity, int32_t clip, bool reset);
/// Living::SetStateAnim (0x5ECB10), on every top state change
void SetAnimalStateAnim(entt::entity entity);

/// Every frame: new animals get their clip; moving ones advance it with the ground covered (fn_0051AF00)
void UpdateAnimalAnimations();

} // namespace openblack::ecs
