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

#include <array>
#include <functional>

#include <glm/vec3.hpp>

/// The drops a sick creature throws: each creature turn its puke plays, twelve drops leave its head, thrown out at
/// random a little, of random brownish tints dimmed by the land's light where the creature stands. Each falls, lands,
/// slides and slows (physics::turn::PuffKind::Drop). Docs: docs/wiki/creature/physiology.md#being-sick.
namespace openblack::creature_puke
{
/// The drops each creature turn
inline constexpr int32_t k_DropsPerTurn = 12;
/// A drop's speed on each axis is a random number from 0 to 99 times the creature's size times this; across, less
/// the creature's size times k_Offset, and downwards on y
inline constexpr float k_Spread = 0.008f;
inline constexpr float k_Offset = 0.4f;
/// A drop's size is the creature's size times this
inline constexpr float k_SizeShare = 0.2f;

struct Drop
{
	glm::vec3 position;
	glm::vec3 velocity;
	uint32_t argb;
	float size;
};

/// The game's local random numbers, from 0 to n - 1
using LocalRandom = std::function<int32_t(int32_t)>;

/// Each drop's tint, 80 to 179 of red, 60 to 139 of green and 20 to 49 of blue, times the light, each channel cut to a
/// whole number (light * tint / 255); the alpha is the light's
[[nodiscard]] uint32_t DropColour(uint32_t light, int32_t red, int32_t green, int32_t blue);

/// The turn's drops from `mouth`, for a creature of `size`, under `light` (the land's light where the creature stands,
/// hazed): each takes its speed across, down and across again, then its red, green and blue, from the local stream
[[nodiscard]] std::array<Drop, k_DropsPerTurn> DropsOf(const glm::vec3& mouth, float size, uint32_t light,
                                                       const LocalRandom& random);
} // namespace openblack::creature_puke
