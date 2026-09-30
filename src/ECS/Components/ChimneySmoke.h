/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// LH3DSmoke (0x110 bytes, LH3DSmoke::Create 0x7F8B60): the smoke of an Abode whose mesh has a chimney, kept in
/// Abode+0x8C (Abode::CallVirtualFunctionsForCreation 0x403200). Simulated and drawn by ecs::chimney_smoke.
struct ChimneySmoke
{
	static constexpr uint32_t k_Puffs = 10; ///< LH3DSprite::Create(10, 0)

	enum class State : uint8_t
	{
		Active = 0, ///< +0xC = 0: puffs are reborn visible
		Dying = 2,  ///< the house emptied: every puff finishes its life and is reborn hidden
		Dead = 3,   ///< nothing drawn any more (LH3DSmoke::AddDrawing 0x7F8D30 skips it)
	};

	struct Puff
	{
		glm::vec3 position {0.0f}; ///< the LH3DSprite's +0
		glm::vec3 velocity {0.0f}; ///< +0x7C + 12i
		int32_t age {0};           ///< +0x20 + 4i, in 1/255 s; i x 90 at the start
		float angle {0.0f};        ///< +0x48 + 4i, Random(0, pi) at the start
		bool clockwise {false};    ///< +0x70 + i: (int)Random(1, 100) & 1, the sense of the spin
		bool hidden {true};        ///< +0x14 + i: 1 at the start
	};

	glm::vec3 position {0.0f};          ///< +0x00: the chimney in world space (GetChimneyPos)
	State state {State::Active};        ///< +0x0C
	uint32_t rgb {0xFFFFFF};            ///< +0x10C: 0x808080 for a workshop
	float ageRemainder {0.0f};          ///< the fraction of the age step kept between frames (openblack, see ChimneySmoke.cpp)
	std::array<Puff, k_Puffs> puffs {}; ///< +0x10: the 10 sprites
};

} // namespace openblack::ecs::components
