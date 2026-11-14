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
#include <bit>

#include "Common/Zoomer.h"

// A zoomer as a saved game keeps it, 48 bytes: the value, the destination, the destination's speed, the speed, a second
// time that is only ever 0, the time, the duration, the start value, the start speed and the three coefficients
// (docs/wiki/engine/matrices-and-zoomer.md#zoomer-lh3dlib)
namespace openblack::ecs::saved_zoomer
{
[[nodiscard]] inline std::array<uint8_t, 48> BytesOf(const Zoomer& zoomer)
{
	const std::array<float, 12> values {zoomer.value, zoomer.destination, zoomer.destinationSpeed, zoomer.speed,      0.0f,
	                                    zoomer.time,  zoomer.duration,    zoomer.startValue,       zoomer.startSpeed, zoomer.c2,
	                                    zoomer.c3,    zoomer.c4};
	return std::bit_cast<std::array<uint8_t, 48>>(values);
}

[[nodiscard]] inline Zoomer ZoomerOf(const std::array<uint8_t, 48>& bytes)
{
	const auto values = std::bit_cast<std::array<float, 12>>(bytes);
	return Zoomer {.value = values[0],
	               .destination = values[1],
	               .destinationSpeed = values[2],
	               .speed = values[3],
	               .time = values[5],
	               .duration = values[6],
	               .startValue = values[7],
	               .startSpeed = values[8],
	               .c2 = values[9],
	               .c3 = values[10],
	               .c4 = values[11]};
}
} // namespace openblack::ecs::saved_zoomer
