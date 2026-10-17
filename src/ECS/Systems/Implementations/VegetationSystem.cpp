/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VegetationSystem.h"

#include <cmath>

#include "Common/GameRandom.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
// The sways speed up and slow down at random every two seconds
constexpr float k_SpeedChangeMilliseconds = 2000.0f;
constexpr float k_MinSpeed = 1.0f;
constexpr float k_MaxSpeed = 2.0f;
// Radians a sway moves on per millisecond at a speed of 1
constexpr float k_PhasePerMillisecond = 0.00106061f;
// How far a sway leans a tree, per unit of its height
constexpr float k_Lean = 0.03f;
// How much further a field's crop leans than a tree
constexpr float k_FieldLean = 1.75f;
} // namespace

void VegetationSystem::Update(std::chrono::duration<float, std::milli> frameTime)
{
	const float milliseconds = frameTime.count();
	_speedTime += milliseconds;
	if (_speedTime > k_SpeedChangeMilliseconds)
	{
		// The C runtime's random numbers, sway by sway
		for (auto& speed : _speeds)
		{
			speed = game_random::crt::Random(k_MinSpeed, k_MaxSpeed);
		}
		_speedTime = 0.0f;
	}
	for (size_t i = 0; i < k_SwayCount; ++i)
	{
		_phases.at(i) += milliseconds * _speeds.at(i) * k_PhasePerMillisecond;
		// The sways lean along the z axis only: the direction they lean in is fixed at 0
		_leans.at(i) = -k_Lean * std::cos(_phases.at(i));
	}
}

float VegetationSystem::GetLean(uint8_t swaySlot) const
{
	return _leans.at(swaySlot % k_SwayCount);
}

glm::mat4 VegetationSystem::GetFieldMatrix(const glm::mat4& model, float scale, uint8_t swaySlot) const
{
	// The up axis sheared along z, its x put upright
	auto swaying = model;
	swaying[1].x = 0.0f;
	swaying[1].z = scale * k_FieldLean * GetLean(swaySlot);
	return swaying;
}
