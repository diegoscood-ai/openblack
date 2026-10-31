/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ExplosionSystem.h"

#include "Camera/CameraShake.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/GroundMarks.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void ExplosionSystem::AddRubble(const glm::vec3& centre, float yaw)
{
	ecs::ground_marks::CreateExplosionMark(centre, yaw);
}

void ExplosionSystem::AddSmoke(const glm::vec3& centre, float size, dust_puff::Kind kind, std::optional<uint32_t> colour)
{
	// The smoke is the puff's first mode, the dust its other one. A colour of all ones keeps the mode's own, so a given
	// colour is passed with another top byte, which the puff does not read
	constexpr uint32_t k_KeepOwnColour = 0xFFFFFFFFu;
	constexpr uint32_t k_GivenColour = 0xFE000000u;
	const int32_t mode = kind == dust_puff::Kind::Smoke ? 0 : 1;
	ecs::disappear_smoke::Create(centre, mode, size,
	                             colour.has_value() ? (k_GivenColour | (*colour & 0xFFFFFFu)) : k_KeepOwnColour);
}

void ExplosionSystem::AddShake(const glm::vec3& position, float radius, float strength, float seconds, bool verticalOnly)
{
	camera_shake::StartEffectShake(position, radius, strength, seconds, verticalOnly);
}

bool ExplosionSystem::IsShaking() const
{
	return !camera_shake::Checkers().empty();
}

void ExplosionSystem::Update(float milliseconds)
{
	ecs::ground_marks::Update(milliseconds);
}

void ExplosionSystem::Reset()
{
	ecs::ground_marks::Clear();
}
