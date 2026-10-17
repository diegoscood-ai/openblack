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

#include <glm/vec3.hpp>

#include "Audio/AnimEffectKeys.h"

/// The dust a creature's steps throw up as their sounds play: a few puffs at the creature, as many as the step is heavy
namespace openblack::creature_dust
{

/// The puffs' colour, before the snow lying there tints it
inline constexpr uint32_t k_Colour = 0x40806040u;

/// How many puffs a sound throws up: a light step 3, a step 6, a stamp 12; none for any other sound
[[nodiscard]] std::optional<int32_t> PuffsFor(audio::SoundAction action);

/// One puff of a creature of a drawn size: its velocity from three local draws, each (draw of 201) - 100 times a
/// twentieth of the drawn size, the first the depth's, the second the height's and the third the width's; and its size,
/// four times the drawn size
struct Puff
{
	glm::vec3 velocity {0.0f};
	float size {0.0f};
};
[[nodiscard]] Puff Make(float drawnSize, const std::function<uint32_t(int32_t)>& localRand);

} // namespace openblack::creature_dust
