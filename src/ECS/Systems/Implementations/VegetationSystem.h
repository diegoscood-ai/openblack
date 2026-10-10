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

#include "ECS/Systems/VegetationInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
class VegetationSystem final: public VegetationInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> frameTime) override;
	[[nodiscard]] float GetLean(uint8_t swaySlot) const override;
	[[nodiscard]] glm::mat4 GetFieldMatrix(const glm::mat4& model, float scale, uint8_t swaySlot) const override;

private:
	/// Phase speed of each sway, 1 to 2, drawn again every two seconds: 0 until the first draw, so there is no sway
	/// for the first two seconds
	std::array<float, k_SwayCount> _speeds {};
	std::array<float, k_SwayCount> _phases {};
	/// Lean of each sway along the z axis, per unit of a tree's height
	std::array<float, k_SwayCount> _leans {};
	/// Milliseconds since the speeds last changed
	float _speedTime {0.0f};
};
} // namespace openblack::ecs::systems
