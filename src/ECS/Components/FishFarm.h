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
#include <optional>

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A fish of the shoal (fn_00824740, 0x24 bytes): an LH3DSprite of misc0.raw lying flat on the water
struct Fish
{
	glm::vec3 position;
	float halfSize;
	float heading;  ///< radians, the direction (cos, 0, sin)
	float speed;    ///< units per second
	float turnRate; ///< radians per second
	float frame;    ///< animation phase 0..15
	float fleeTime; ///< seconds left fleeing a splash
};

/// The shoal of a fish farm (FishFarm::CallVirtualFunctionsForCreation 0x52CC10, 0x68 bytes, list 0xEB99F4)
struct FishShoal
{
	static constexpr size_t k_FishCount = 15; ///< 15 x [+0x64] (= 1)
	static constexpr float k_Range = 7.0f;    ///< +0x58: the target wanders this far from the centre

	glm::vec3 centre;
	glm::vec3 target;
	float timer {0.0f}; ///< seconds until a new target
	std::array<Fish, k_FishCount> fish;
	uint8_t alpha {255}; ///< this frame, from the camera distance
	bool visible {false};
};

/// A fish farm (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM). No shoal when no sea was found around it.
struct FishFarm
{
	std::optional<FishShoal> shoal;
};

} // namespace openblack::ecs::components
