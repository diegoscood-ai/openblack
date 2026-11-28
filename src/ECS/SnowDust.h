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

#include <glm/vec3.hpp>

/// Dust thrown up from the land takes the colour of the land's light where snow lies: the deeper the snow, the nearer
/// its colour comes to the land's colour of the moment
namespace openblack::ecs::snow_dust
{

/// How much snow lies at a point, cut to a whole number and kept within 0 to 255
[[nodiscard]] int32_t SnowAt(glm::vec3 point);

/// A dust colour where it is made: blended towards the land's light of the moment by the snow there, its alpha kept
[[nodiscard]] uint32_t Tint(uint32_t argb, int32_t snow);

/// The size of a puff of dust a building's piece sheds as it breaks off, from the snow lying where the puff is made
/// (SnowAt): 2 where none lies, growing with the snow to 4 where it lies 255 deep or more
[[nodiscard]] constexpr float PieceDustSize(int32_t snow) noexcept
{
	return (static_cast<float>(snow) * (1.0f / 255.0f) + 1.0f) * 2.0f;
}

} // namespace openblack::ecs::snow_dust
