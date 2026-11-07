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

#include <chrono>

#include <glm/mat4x4.hpp>

namespace openblack::ecs::systems
{
/// The wind the trees and the ripe fields sway in: 16 sways, each a slow back and forth whose speed changes at random
/// every two seconds, leaning along the world's z axis whatever the weather. A tree follows the sway its facing picks
class VegetationInterface
{
public:
	/// Number of sways that trees and fields share
	static constexpr uint8_t k_SwayCount = 16;

	virtual ~VegetationInterface() = default;
	/// Moves the sways on by the time that has passed: the frame's real time, which goes on while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> frameTime) = 0;
	/// How far a sway leans a tree along the world's z axis, per unit of its height
	[[nodiscard]] virtual float GetLean(uint8_t swaySlot) const = 0;
	/// The matrix to draw a fully grown field with, its crop swaying further than trees do. model places
	/// the field, which is scale times its mesh's size, and swaySlot is which of the shared sways it follows.
	[[nodiscard]] virtual glm::mat4 GetFieldMatrix(const glm::mat4& model, float scale, uint8_t swaySlot) const = 0;
};
} // namespace openblack::ecs::systems
