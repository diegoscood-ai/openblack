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

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "Graphics/ArgbColour.h"

// The player's hand inside the temple. The temple draws it itself, once a frame inside the room's draw, with its colour
// set to a pale grey and no specular, instead of the land's light at its place and the land draw's half again. Pure:
// values only. Wiki: docs/wiki/rendering/model-lighting.md.
namespace openblack::graphics::temple_hand
{

/// The colour the temple sets on the hand before drawing it (alpha 0xFF, grey 0xE0)
inline constexpr uint32_t k_Colour = 0xFFE0E0E0u;
/// And its specular
inline constexpr uint32_t k_Specular = 0;

/// The row the temple draws the hand with: its row and colour column as the frame's instances upload them, the colour
/// and specular set to the temple's (drawn instead of the land light, without the haze); the column's other fields as
/// they are. Drawn from its bones as finished in the world (`worldBones`), its matrix is the identity, the columns' w
/// (the row's other data) kept, as the uploaded rows hold it; otherwise its own matrix
[[nodiscard]] inline std::array<glm::vec4, 5> Row(const glm::mat4& handRow, const glm::vec4& handColours,
                                                  bool worldBones) noexcept
{
	glm::mat4 matrix = handRow;
	if (worldBones)
	{
		matrix = glm::mat4(1.0f);
		for (int i = 0; i < 3; ++i)
		{
			matrix[i].w = handRow[i].w;
		}
	}
	std::array<glm::vec4, 5> row {matrix[0], matrix[1], matrix[2], matrix[3], handColours};
	argb_colour::PackInstanceColour(row[4], k_Colour);
	argb_colour::PackInstanceSpecular(row[4], k_Specular);
	return row;
}

} // namespace openblack::graphics::temple_hand
