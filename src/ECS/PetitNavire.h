/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::petit_navire
{

/// PetitNavire (JCMisc.cpp, 0x68 bytes, the one boat of [0xD19CB4]): the missionaries' boat of Land 1, made by the CHL
/// PLAY_JC_SPECIAL(6) (fn_005DF9C0 case 6: new PetitNavire(0)). Mode 0 is the launch: MSH_O_ARK slides down the dry
/// dock along Data\MISC\Boat1.anm while five sailors push, with ScriptSFX creak / slide / splash and dust and spray;
/// near the end of the clip it turns into mode 1, the crossing: the hull rocks on Boat2.anm and sails from
/// (1456.54, 0, 3263.06) at 5 units a second along -(1, 0, 1)/sqrt 2 for 60 s with the cow, the grain pile and five
/// people on deck and a wake of five flat smoke sprites. docs/bw1-notes/objects-and-resources.md "Barco".

/// PetitNavire::PetitNavire(mode) 0x5E1020: an existing boat is freed first (fn_005E13C0)
void Create(int32_t mode);

/// One frame (g_game_time_inc milliseconds of game time, whole as in the original): PetitNavire::PreDraw 0x5DFF20
/// (from GLandscape::Draw 0x5E490F: the clip, the hull's matrix, the reflection) then PostDraw 0x5E03F0 (from
/// fn_005E5CD0 0x5E6250: sounds, dust, the hull, the people and the wake)
void Update(float gameMilliseconds);

/// The hull while a boat exists, drawn into the reflection by PreDraw's DrawUnderWater (vt+0x118) in 0xFF303070
[[nodiscard]] entt::entity GetReflectedHull();
constexpr uint32_t k_ReflectionColour = 0xFF303070u;

/// A sprite of the mode 1 wake (LH3DSprite +0x4C[i], flag 0x40: flat on XZ, smoke material [0xEA1ABC], mode 6)
struct WakeSprite
{
	glm::vec3 position;
	float half;     ///< +0x0C
	float aspect;   ///< +0x10: the z half is half x aspect
	float angle;    ///< +0x14: the turn about Y
	uint8_t cell;   ///< +0x28 & 0x3F
	uint32_t argb;  ///< +0x20
};
/// The wake sprites AddDrawing'd this frame
[[nodiscard]] const std::vector<WakeSprite>& GetWake();

/// Test hook OPENBLACK_TEST_JC_SPECIAL="6[,mode[,frames[,fast forward ms]]]": PLAY_JC_SPECIAL(6) that many frames after
/// the landscape exists (mode 1 starts the crossing at once), then (test only) that much game time at once in steps of
/// 33 ms, for screenshots at a given moment. OPENBLACK_BOAT_TRACE=1 logs the boat every 500 ms.
void RunDebugHook();

} // namespace openblack::ecs::petit_navire
